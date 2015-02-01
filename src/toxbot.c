/*  toxbot.c
 *
 *
 *  Copyright (C) 2014 toxbot All Rights Reserved.
 *
 *  This file is part of toxbot.
 *
 *  toxbot is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  toxbot is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with toxbot. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <signal.h>

#include <tox/tox.h>
#include <tox/toxav.h>

#include <dirent.h>

#include "misc.h"
#include "commands.h"
#include "toxbot.h"
#include "groupchats.h"

#define VERSION "0.2.1"
#define FRIEND_PURGE_INTERVAL 3600

bool FLAG_EXIT = false;    /* set on SIGINT */
char *DATA_FILE = "toxbot_save";
char *MASTERLIST_FILE = "masterkeys";
char *SETTINGS_FILE = "settings";
char *FRIENDS_FILE = "friends";

struct Tox_Bot Tox_Bot;

static void init_toxbot_state(void)
{
    Tox_Bot.start_time = (uint64_t) time(NULL);
    Tox_Bot.default_groupnum = 0;
    Tox_Bot.chats_idx = 0;

    /* 1 year default; anything lower should be explicitly set until we have a config file */
    Tox_Bot.inactive_limit = 31536000;
}

static void catch_SIGINT(int sig)
{
    FLAG_EXIT = true;
}

static void exit_groupchats(Tox *m, uint32_t numchats)
{
    memset(Tox_Bot.g_chats, 0, Tox_Bot.chats_idx * sizeof(struct Group_Chat));
    realloc_groupchats(0);

    int32_t *groupchat_list = malloc(numchats * sizeof(int32_t));

    if (groupchat_list == NULL)
        return;

    if (tox_get_chatlist(m, groupchat_list, numchats) == 0) {
        free(groupchat_list);
        return;
    }

    uint32_t i;

    for (i = 0; i < numchats; ++i)
        tox_del_groupchat(m, groupchat_list[i]);

    free(groupchat_list);
}

static void exit_toxbot(Tox *m)
{
    uint32_t numchats = tox_count_chatlist(m);

    if (numchats)
        exit_groupchats(m, numchats);

    save_data(m, DATA_FILE);
    tox_kill(m);
    exit(EXIT_SUCCESS);
}

/* Returns true if friendnumber's Tox ID is in the masterkeys list, false otherwise.
   Note that it only compares the public key portion of the IDs. */
bool friend_is_master(Tox *m, int32_t friendnumber)
{
    if (!file_exists(MASTERLIST_FILE)) {
        FILE *fp = fopen(MASTERLIST_FILE, "w");

        if (fp == NULL) {
            fprintf(stderr, "Warning: failed to create masterkeys file\n");
            return false;
        }

        fclose(fp);
        fprintf(stderr, "Warning: creating new masterkeys file. Did you lose the old one?\n");
        return false;
    }

    FILE *fp = fopen(MASTERLIST_FILE, "r");

    if (fp == NULL) {
        fprintf(stderr, "Warning: failed to read masterkeys file\n");
        return false;
    }

    char friend_key[TOX_CLIENT_ID_SIZE];
    tox_get_client_id(m, friendnumber, (uint8_t *) friend_key);
    char id[256];

    while (fgets(id, sizeof(id), fp)) {
        int len = strlen(id);

        if (--len < TOX_CLIENT_ID_SIZE)
            continue;

        char *key_bin = hex_string_to_bin(id);

        if (memcmp(key_bin, friend_key, TOX_CLIENT_ID_SIZE) == 0) {
            free(key_bin);
            fclose(fp);
            return true;
        }

        free(key_bin);
    }

    fclose(fp);
    return false;
}

/* START CALLBACKS */
static void cb_friend_request(Tox *m, const uint8_t *public_key, const uint8_t *data, uint16_t length,
                              void *userdata)
{
    tox_add_friend_norequest(m, public_key);
    save_data(m, DATA_FILE);
}

static void cb_friend_message(Tox *m, int32_t friendnumber, const uint8_t *string, uint16_t length,
                              void *userdata)
{
    const char *outmsg;
    char message[TOX_MAX_MESSAGE_LENGTH];
    length = copy_tox_str(message, sizeof(message), (const char *) string, length);
    message[length] = '\0';

    if (length && execute(m, friendnumber, message, length) == -1) {
        outmsg = "Ungültiger Befehl. Bitte gib hilfe ein, um dir die Befehle anzeigen zu lassen.";
        tox_send_message(m, friendnumber, (uint8_t *) outmsg, strlen(outmsg));
    }
}

static void cb_group_invite(Tox *m, int32_t friendnumber, uint8_t type, const uint8_t *group_pub_key, uint16_t length,
                            void *userdata)
{
    if (!friend_is_master(m, friendnumber))
        return;

    char name[TOX_MAX_NAME_LENGTH];
    int len = tox_get_name(m, friendnumber, (uint8_t *) name);
    name[len] = '\0';

    int groupnum = -1;

    if (type == TOX_GROUPCHAT_TYPE_TEXT)
        groupnum = tox_join_groupchat(m, friendnumber, group_pub_key, length);
    else if (type == TOX_GROUPCHAT_TYPE_AV)
        groupnum = toxav_join_av_groupchat(m, friendnumber, group_pub_key, length, NULL, NULL);

    if (groupnum == -1) {
        fprintf(stderr, "Invite from %s failed (core failure)\n", name);
        return;
    }

    if (group_add(groupnum, type, NULL) == -1) {
        fprintf(stderr, "Invite from %s failed (group_add failed)\n", name);
        tox_del_groupchat(m, groupnum);
        return;
    }

    printf("Accepted groupchat invite from %s [%d]\n", name, groupnum);
}

static void cb_group_titlechange(Tox *m, int groupnumber, int peernumber, const uint8_t *title, uint8_t length,
                                 void *userdata)
{
    char message[TOX_MAX_MESSAGE_LENGTH];
    length = copy_tox_str(message, sizeof(message), (const char *) title, length);

    int idx = group_index(groupnumber);

    if (idx == -1)
        return;

    memcpy(Tox_Bot.g_chats[idx].title, message, length + 1);
    Tox_Bot.g_chats[idx].title_len = length;
}

/* END CALLBACKS */

int save_data(Tox *m, const char *path)
{
    if (path == NULL)
        goto on_error;

    int len = tox_size(m);
    char *buf = malloc(len);

    if (buf == NULL)
        exit(EXIT_FAILURE);

    tox_save(m, (uint8_t *) buf);

    FILE *fp = fopen(path, "wb");

    if (fp == NULL) {
        free(buf);
        goto on_error;
    }

    if (fwrite(buf, len, 1, fp) != 1) {
        free(buf);
        fclose(fp);
        goto on_error;
    }

    free(buf);
    fclose(fp);
    return 0;

on_error:
    fprintf(stderr, "Warning: save_data failed\n");
    return -1;
}

static int load_data(Tox *m, char *path)
{
    FILE *fp = fopen(path, "rb");

    if (fp == NULL) {
        if (save_data(m, path) != 0)
            return -1;

        return 0;
    }

    off_t len = file_size(path);

    if (len == -1) {
        fclose(fp);
        return -1;
    }

    char *buf = malloc(len);

    if (buf == NULL) {
        fclose(fp);
        return -1;
    }

    if (fread(buf, len, 1, fp) != 1) {
        free(buf);
        fclose(fp);
        return -1;
    }

    if (tox_load(m, (uint8_t *) buf, len) == 1) {
        fprintf(stderr, "Data file is encrypted\n");
        exit(EXIT_SUCCESS);
    }

    free(buf);
    fclose(fp);
    return 0;
}

static Tox *init_tox(void)
{
    Tox_Options tox_opts;
    memset(&tox_opts, 0, sizeof(Tox_Options));

    tox_opts.ipv6enabled = 1;

    Tox *m = tox_new(&tox_opts);

    if (m == NULL)
        return NULL;

    tox_callback_friend_request(m, cb_friend_request, NULL);
    tox_callback_friend_message(m, cb_friend_message, NULL);
    tox_callback_group_invite(m, cb_group_invite, NULL);
    tox_callback_group_title(m, cb_group_titlechange, NULL);

    const char *statusmsg = "Send me the the command 'help' for more info";
    tox_set_status_message(m, (uint8_t *) statusmsg, strlen(statusmsg));
    tox_set_name(m, (uint8_t *) "ToxBot", strlen("ToxBot"));

    return m;
}

/* TODO: hardcoding is bad stop being lazy */
static struct toxNodes {
    const char *ip;
    uint16_t    port;
    const char *key;
} nodes[] = {
    { "192.254.75.98",   33445, "951C88B7E75C867418ACDB5D273821372BB5BD652740BCDF623A4FA293E75D2F" },
    { "192.210.149.121", 33445, "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67" },
    { "195.154.119.113", 33445, "E398A69646B8CEACA9F0B84F553726C1C49270558C57DF5F3C368F05A7D71354" },
    { "46.38.239.179",   33445, "F5A1A38EFB6BD3C2C8AF8B10D85F0F89E931704D349F1D0720C3C4059AF2440A" },
    { "31.7.57.236",     443,   "2A4B50D1D525DA2E669592A20C327B5FAD6C7E5962DC69296F9FEC77C4436E4E" },
    { NULL, 0, NULL },
};

static void bootstrap_DHT(Tox *m)
{
    int i;

    for (i = 0; nodes[i].ip; ++i) {
        char *key = hex_string_to_bin(nodes[i].key);

        if (tox_bootstrap_from_address(m, nodes[i].ip, nodes[i].port, (uint8_t *) key) != 1)
            fprintf(stderr, "Failed to bootstrap DHT via: %s %d\n", nodes[i].ip, nodes[i].port);

        free(key);
    }
}

static void print_profile_info(Tox *m)
{
    printf("ToxBot version %s\n", VERSION);
    printf("ID: ");
    char address[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(m, (uint8_t *) address);
    int i;

    for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; ++i) {
        char d[3];
        snprintf(d, sizeof(d), "%02X", address[i] & 0xff);
        printf("%s", d);
    }

    printf("\n");

    char name[TOX_MAX_NAME_LENGTH];
    uint16_t len = tox_get_self_name(m, (uint8_t *) name);
    name[len] = '\0';
    uint32_t numfriends = tox_count_friendlist(m);
    printf("Name: %s\n", name);
    printf("Kontakte: %d\n", numfriends);
    printf("Inaktive Nutzer werden nach %"PRIu64" Tagen entfernt\n", Tox_Bot.inactive_limit / SECONDS_IN_DAY);
}

static void purge_inactive_friends(Tox *m)
{
    uint32_t i;
    uint64_t cur_time = (uint64_t) time(NULL);
    uint32_t numfriends = tox_count_friendlist(m);

    if (numfriends == 0)
        return;

    int32_t *friend_list = malloc(numfriends * sizeof(int32_t));

    if (friend_list == NULL)
        exit(EXIT_FAILURE);

    if (tox_get_friendlist(m, friend_list, numfriends) == 0) {
        free(friend_list);
        return;
    }

    for (i = 0; i < numfriends; ++i) {
        uint32_t friendnum = friend_list[i];

        if (!tox_friend_exists(m, friendnum))
            continue;

        uint64_t last_online = tox_get_last_online(m, friendnum);

        if (cur_time - last_online > Tox_Bot.inactive_limit)
            tox_del_friend(m, friendnum);
    }

    free(friend_list);
}

#define REC_TOX_DO_LOOPS_PER_SEC 25

/* Adjusts usleep value so that tox_do runs close to the recommended number of times per second */
static useconds_t optimal_msleepval(uint64_t *looptimer, uint64_t *loopcount, uint64_t cur_time, useconds_t msleepval)
{
    useconds_t new_sleep = msleepval;
    ++(*loopcount);

    if (*looptimer == cur_time)
        return new_sleep;

    if (*loopcount != REC_TOX_DO_LOOPS_PER_SEC)
        new_sleep *= (double) *loopcount / REC_TOX_DO_LOOPS_PER_SEC;

    *looptimer = cur_time;
    *loopcount = 0;
    return new_sleep;
}

//check ID
/*
bool checkID(int32_t id){
    Messenger *m = tox;
    if (m_addfriend_norequest(m, id)){
        return true;
    } else {
        return false;
    }
}*/

int main(int argc, char *argv[])
{
    signal(SIGINT, catch_SIGINT);
    umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    Tox *m = init_tox();

    //Flags and helpmenue

    printf("\n");
    printf(" _____           ____        _   \n");
    printf("|_   _|____  __ | __ )  ___ | |_ \n");
    printf("  | |/ _ \\ \\/ / |  _ \\ / _ \\| __|  \n");
    printf("  | | (_) >  <  | |_) | (_) | |_   \n");
    printf("  |_|\\___/_/\\_\\ |____/ \\___/ \\__| \n\n");

    if (argc > 1 && (strcmp(argv[1], "-b")==0 || strcmp(argv[1], "--background")==0)){
        printf("\nStarte Bot im Hintergrund...\n\n");
        system("./toxbot> /dev/null 2&>1&");
        return 0;
    }

    if(argc > 1 && (strcmp(argv[1], "--help")==0 || strcmp(argv[1], "-h")==0)){
        printf("\ntoxbot [-Option/--Option]\n\nMögliche Optionen:\n\t-h / --help \t\t\t Zeigt diese Nachricht\n\t-b / --background\t\t Startet den Bot im Hintergrund\n\t-a [ID]/ --addmaster [ID]\t Fügt die ID der Masterdatei hinzu\n\t-s / --save\t\t\t Macht ein Backup des bestehenden Bots in ToxBot/Backup/toxbot_save\n\t-r / --restore\t\t\t Stellt einen Bot aus ToxBot/Backup/toxbot_save wieder her\n\t-q / --quit\t\t\t Beendet alle ToxBot-Instanzen\n\nTox-Bot Fork von dj95. Originaler Tox-Bot https://github.com/JFreegman/ToxBot \n\n");
        return 0;
    }

    if (argc > 1 && (strcmp(argv[1], "-q")==0 || strcmp(argv[1], "--quit")==0)){
        printf("\nBeende alle ToxBot-Instanzen im Hintergrund!\n\n");
        system("killall toxbot");
        return 0;
    }

    if (argc > 1 && (strcmp(argv[1], "-s")==0 || strcmp(argv[1], "--save")==0)){
        system("mkdir Backup && cp toxbot_save Backup/");
        printf("\nBackup erfolgreich erstellt\n\n");
        return 0;
    }

    if (argc > 1 &&(strcmp(argv[1], "-r")==0 || strcmp(argv[1], "--restore")==0)){
        system("cd Backup && cp toxbot_save ../toxbot_save");
        printf("\nBackup erfolgreich wiederhergestellt\n\n");
        return 0;
    }

    if (argc > 1 && (strcmp(argv[1], "-a")==0 || strcmp(argv[1], "--addmaster")==0)){
        if(argv[2]!=NULL){
            int len = strlen(argv[2]) + strlen("echo  >> masterkeys");
            if (len != (76 + strlen("echo  >> masterkeys"))){
                printf("Ungültiges ID-Format! Bitte die 76-stellige ID eingeben.\n");
                return 1;
            }
            char cmd[len];
            strncpy(cmd, "echo ", len);
            strncat(cmd, argv[2], len);
            strncat(cmd, " >> masterkeys", strlen(" >> masterkeys"));
            system(cmd);
            printf("\nDie ID wurde erfolgreich hinzugefügt\n\n");
        } else {
            printf("\nID fehtl! Benutze -h um die Hilfe zu zeigen.\n\n");
        }
        return 0;
    }

    if (m == NULL) {
        fprintf(stderr, "Tox Netzwerk konnte nicht initialisiert werden.\n");
        exit(EXIT_FAILURE);
    }

    if (load_data(m, DATA_FILE) == -1)
        fprintf(stderr, "Daten konnten nicht geladen werden\n");

    init_toxbot_state();
    print_profile_info(m);
    bootstrap_DHT(m);

    uint64_t looptimer = (uint64_t) time(NULL);
    uint64_t last_purge = 0;
    useconds_t msleepval = 40000;
    uint64_t loopcount = 0;

    while (!FLAG_EXIT) {
        uint64_t cur_time = (uint64_t) time(NULL);

        if (timed_out(last_purge, cur_time, FRIEND_PURGE_INTERVAL)) {
            purge_inactive_friends(m);
            save_data(m, DATA_FILE);
            last_purge = cur_time;
        }

        tox_do(m);

        msleepval = optimal_msleepval(&looptimer, &loopcount, cur_time, msleepval);
        usleep(msleepval);
    }

    exit_toxbot(m);
    return 0;
}
