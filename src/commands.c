/*  commands.c
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
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <time.h>

#include <tox/tox.h>
#include <tox/toxav.h>

#include "toxbot.h"
#include "misc.h"
#include "groupchats.h"

#define MAX_COMMAND_LENGTH TOX_MAX_MESSAGE_LENGTH
#define MAX_NUM_ARGS 4

extern char *DATA_FILE;
extern char *MASTERLIST_FILE;
extern char *SETTINGS_FILE;
extern struct Tox_Bot Tox_Bot;

static void authent_failed(Tox *m, int friendnum)
{
    const char *outmsg = "Du...Du bist nicht mein Master...";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
}

static void cmd_default(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Fehler: Raumnummer erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int groupnum = atoi(argv[1]);

    if ((groupnum == 0 && strcmp(argv[1], "0")) || groupnum < 0) {
        outmsg = "fehler: Ungültige Raumnummer";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    Tox_Bot.default_groupnum = groupnum;

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Standard Gruppennummer auf %d geändert", groupnum);
    tox_send_message(m, friendnum, (uint8_t *) msg, strlen(msg));

    char name[TOX_MAX_NAME_LENGTH];
    int len = tox_get_name(m, friendnum, (uint8_t *) name);
    name[len] = '\0';

    printf("Standard Gruppennummer auf %d geändert von %s", groupnum, name);
}

static void cmd_gmessage(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Fehler: Gruppen nummer erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (argc < 2) {
        outmsg = "Fehler: Nachricht erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Fehler: Ungültige Gruppennummer";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (group_index(groupnum) == -1) {
        outmsg = "Fehler: Ungültige Gruppennummer";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (argv[2][0] != '\"') {
        outmsg = "Fehler: Nachricht muss in Anführungszeichen stehen";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    /* remove opening and closing quotes */
    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "%s", &argv[2][1]);
    int len = strlen(msg) - 1;
    msg[len] = '\0';

    if (tox_group_message_send(m, groupnum, (uint8_t *) msg, strlen(msg)) == -1) {
        outmsg = "Fehler: Konnte Nachricht nicht senden.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';
    outmsg = "Nachricht gesendet.";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
    printf("<%s> Nachricht an Gruppe %d: %s\n", name, groupnum, msg);
}

static void cmd_group(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Bitte setze den Gruppentyp auf: audio or text";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    uint8_t type = TOX_GROUPCHAT_TYPE_AV ? !strcasecmp(argv[1], "audio") : TOX_GROUPCHAT_TYPE_TEXT;

    char name[TOX_MAX_NAME_LENGTH];
    int len = tox_get_name(m, friendnum, (uint8_t *) name);
    name[len] = '\0';

    int groupnum = -1;

    if (type == TOX_GROUPCHAT_TYPE_TEXT)
        groupnum = tox_add_groupchat(m);
    else if (type == TOX_GROUPCHAT_TYPE_AV)
        groupnum = toxav_add_av_groupchat(m, NULL, NULL);

    if (groupnum == -1) {
        printf("Gruppenerstellung von %s konnte nicht initialisiert werden\n", name);
        outmsg = "Gruppenchat konnte nicht initialisiert werden.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    const char *password = argc >= 2 ? argv[2] : NULL;

    if (password && strlen(argv[2]) >= MAX_PASSWORD_SIZE) {
        printf("Gruppenerstellung von %s fehlerhaft: Passwort zu lang\n", name);
        outmsg = "Gruppenchat konnte nicht initialisiert werden: Passwort zu lang";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (group_add(groupnum, type, password) == -1) {
        printf("Gruppenerstellung von %s fehlerhaft\n", name);
        outmsg = "Gruppe konnte nicht erstellt werden";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        tox_del_groupchat(m, groupnum);
        return;
    }

    const char *pw = password ? " (Password geschützt)" : "";
    printf("Gruppenchat %d wurde erstellt von %s%s\n", groupnum, name, pw);

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Gruppenchat %d erstallt %s", groupnum, pw);
    tox_send_message(m, friendnum, (uint8_t *) msg, strlen(msg));
}

static void cmd_help(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    outmsg = "info : Zeigt dir den Status des Bots, sowie die Gruppenchats";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    outmsg = "id : Zeigt dir die Tox-ID des Bots";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    outmsg = "hallo : Lädt dich in den bestehenden Gruppen-Chat";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    outmsg = "hallo <n> <p> : Lädt dich in eine mit einem Passwort geschützte Gruppe ein";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    outmsg = "register <n> <id> : Speichert deine Kontaktdaten im Telefonbuch";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    outmsg = "kontakte : Zeigt alle registrierten Kontakte des Bots an";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    if (friend_is_master(m, friendnum)) {
        outmsg = "Für Master-Kommands gucke in die Commands.txt oder frage den Admin des Bots";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
    }
}

static void cmd_id(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    char outmsg[TOX_FRIEND_ADDRESS_SIZE * 2 + 1];
    char address[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(m, (uint8_t *) address);
    int i;

    for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; ++i) {
        char d[3];
        sprintf(d, "%02X", address[i] & 0xff);
        memcpy(outmsg + i * 2, d, 2);
    }

    outmsg[TOX_FRIEND_ADDRESS_SIZE * 2] = '\0';
    tox_send_message(m, friendnum, (uint8_t *) outmsg, TOX_FRIEND_ADDRESS_SIZE * 2);
}

static void cmd_info(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    //Owner-File
    char owner[50];
    int len = sizeof(owner);

    FILE *in = fopen(SETTINGS_FILE, "rb");
    char fInput[len + 2];

    if (in != NULL){
        int count = 0;
        while(fgets(fInput, (len), in) != NULL) {
            if (count == 0){
                strncpy(owner, fInput+7, len-7);
            }
            count++;
        }
        fclose(in);
    } else {
        FILE *out = fopen(SETTINGS_FILE, "w");
        fprintf(out, "%s\n", "Owner: Tox-Bot(Ändere den Eigentümer im settings-File)");
        strncpy(owner, "Tox-Bot(Ändere den Eigentümer im settings-File)", len);
        fclose(out);
    }

    char outmsg[MAX_COMMAND_LENGTH];
    char timestr[64];

    uint64_t curtime = (uint64_t) time(NULL);
    get_elapsed_time_str(timestr, sizeof(timestr), curtime - Tox_Bot.start_time);
    snprintf(outmsg, sizeof(outmsg), "Betriebszeit: %s", timestr);
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    uint32_t numfriends = tox_count_friendlist(m);
    uint32_t numonline = tox_get_num_online_friends(m);
    snprintf(outmsg, sizeof(outmsg), "Freunde: %d (%d online)", numfriends, numonline);
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    snprintf(outmsg, sizeof(outmsg), "Eigentümer: %s", owner);
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    snprintf(outmsg, sizeof(outmsg), "Inaktive Freunde werden nach %"PRIu64" Tagen entfernt",
                                      Tox_Bot.inactive_limit / SECONDS_IN_DAY);
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    /* List active group chats and number of peers in each */
    uint32_t numchats = tox_count_chatlist(m);

    if (numchats == 0) {
        tox_send_message(m, friendnum, (uint8_t *) "Keine aktiven Gruppenchats", strlen("Keine aktiven Gruppenchats"));
        return;
    }

    int32_t *groupchat_list = malloc(numchats * sizeof(int32_t));

    if (groupchat_list == NULL)
        exit(EXIT_FAILURE);

    if (tox_get_chatlist(m, groupchat_list, numchats) == 0) {
        free(groupchat_list);
        return;
    }

    uint32_t i;

    for (i = 0; i < numchats; ++i) {
        uint32_t groupnum = groupchat_list[i];
        int num_peers = tox_group_number_peers(m, groupnum);

        if (num_peers != -1) {
            int idx = group_index(groupnum);
            const char *title = Tox_Bot.g_chats[idx].title_len
                              ? Tox_Bot.g_chats[idx].title : "Keiner";
            const char *type = tox_group_get_type(m, groupnum) == TOX_GROUPCHAT_TYPE_TEXT ? "Text" : "Audio";
            snprintf(outmsg, sizeof(outmsg), "Gruppe %d | %s | Teilnehmer: %d | Name: %s", groupnum, type,
                                                                                      num_peers, title);
            tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        }
    }

    free(groupchat_list);
}

static void cmd_invite(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;
    int groupnum = Tox_Bot.default_groupnum;

    if (argc >= 1) {
        groupnum = atoi(argv[1]);

        if (groupnum == 0 && strcmp(argv[1], "0")) {
            outmsg = "Fehler: Ungültige Gruppennummer.";
            tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
            return;
        }
    }

    int idx = group_index(groupnum);

    if (idx == -1) {
        outmsg = "Die Gruppe existiert nicht.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int has_pass = Tox_Bot.g_chats[idx].has_pass;

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    const char *passwd = NULL;

    if (argc >= 2)
        passwd = argv[2];

    if (has_pass && (!passwd || strcmp(argv[2], Tox_Bot.g_chats[idx].password) != 0)) {
        fprintf(stderr, "Fehler %s in die Gruppe %d einzuladen(falsches Passwort)\n", name, groupnum);
        outmsg = "Falsches Passwort.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (tox_invite_friend(m, friendnum, groupnum) == -1) {
        fprintf(stderr, "Fehler %s in die Gruppe %d einzuladen\n", name, groupnum);
        outmsg = "Einladung gescheitert. Bitte melde das Problem im irc #tox @freenode.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    printf("Hab %s in Gruppe %d eingeladen\n", name, groupnum);
}

static void cmd_leave(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Fehler: Gruppennummer erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Fehler: Ungültige Gruppennummer";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (tox_del_groupchat(m, groupnum) == -1) {
        outmsg = "Fehler: Ungültige Gruppennummer";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    char msg[MAX_COMMAND_LENGTH];
    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    group_leave(groupnum);

    printf("Verlasse Gruppe %d (%s)\n", groupnum, name);
    snprintf(msg, sizeof(msg), "Verlasse Gruppe %d", groupnum);
    tox_send_message(m, friendnum, (uint8_t *) msg, strlen(msg));
}

static void cmd_master(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Fehler: Tox ID erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    const char *id = argv[1];

    if (strlen(id) != TOX_FRIEND_ADDRESS_SIZE * 2) {
        outmsg = "Fehler: Ungültige Tox ID";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    FILE *fp = fopen(MASTERLIST_FILE, "a");

    if (fp == NULL) {
        outmsg = "Fehler: Kann masterkey Datei nicht finden";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    fprintf(fp, "%s\n", id);
    fclose(fp);

    char name[TOX_MAX_NAME_LENGTH];
    int len = tox_get_name(m, friendnum, (uint8_t *) name);
    name[len] = '\0';

    printf("%s hat Master hinzugefügt: %s\n", name, id);
    outmsg = "ID zu masterkeys hinzugefügt";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
}

static void cmd_name(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Fehler: Name erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    int len = 0;

    if (argv[1][0] == '\"') {    /* remove opening and closing quotes */
        snprintf(name, sizeof(name), "%s", &argv[1][1]);
        len = strlen(name) - 1;
    } else {
        snprintf(name, sizeof(name), "%s", argv[1]);
        len = strlen(name);
    }

    name[len] = '\0';
    tox_set_name(m, (uint8_t *) name, (uint16_t) len);

    char m_name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) m_name);
    m_name[nlen] = '\0';

    printf("%s ändert Name zu %s\n", m_name, name);
    save_data(m, DATA_FILE);
}

static void cmd_passwd(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (argc < 1) {
        outmsg = "Fehler: Gruppennummer erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Fehler: Ungültige Gruppennummer";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int idx = group_index(groupnum);

    if (idx == -1) {
        outmsg = "Fehler: Ungültige Gruppennummer";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    /* no password */
    if (argc < 2) {
        Tox_Bot.g_chats[idx].has_pass = false;
        memset(Tox_Bot.g_chats[idx].password, 0, MAX_PASSWORD_SIZE);

        outmsg = "Kein Passwort gesetzt";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        printf("Kein Passwort für Gruppe %d von %s gesetzt\n", groupnum, name);
        return;
    }

    if (strlen(argv[2]) >= MAX_PASSWORD_SIZE) {
        outmsg = "Passwort zu lang";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    Tox_Bot.g_chats[idx].has_pass = true;
    snprintf(Tox_Bot.g_chats[idx].password, sizeof(Tox_Bot.g_chats[idx].password), "%s", argv[2]);

    outmsg = "Passwort geändert";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
    printf("Passwort für Gruppe %d geändert von %s\n", groupnum, name);

}

static void cmd_purge(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Fehler: Nummer > 0 erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    uint64_t days = (uint64_t) atoi(argv[1]);

    if (days <= 0) {
        outmsg = "Fehler: Nummer > 0 erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    uint64_t seconds = days * SECONDS_IN_DAY;
    Tox_Bot.inactive_limit = seconds;

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Entfernen Zeit auf %"PRIu64" Tage geändert", days);
    tox_send_message(m, friendnum, (uint8_t *) msg, strlen(msg));

    printf("Entfernen Zeit auf %"PRIu64" Tage geändert von %s\n", days, name);
}

static void cmd_status(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Fehler: Status erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    TOX_USERSTATUS type;
    const char *status = argv[1];

    if (strcasecmp(status, "online") == 0)
        type = TOX_USERSTATUS_NONE;
    else if (strcasecmp(status, "away") == 0)
        type = TOX_USERSTATUS_AWAY;
    else if (strcasecmp(status, "busy") == 0)
        type = TOX_USERSTATUS_BUSY;
    else {
        outmsg = "Ungültiger Status. Gültige Statusmeldungen sind: online, busy und away.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    tox_set_user_status(m, type);

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    printf("%s ändert Status auf %s\n", name, status);
    save_data(m, DATA_FILE);
}

static void cmd_statusmessage(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Fehler: Nachricht erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (argv[1][0] != '\"') {
        outmsg = "Fehler: Nachricht muss in Anführungszeichen stehen";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    /* remove opening and closing quotes */
    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "%s", &argv[1][1]);
    int len = strlen(msg) - 1;
    msg[len] = '\0';

    tox_set_status_message(m, (uint8_t *) msg, len);

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    printf("%s ändert Status auf \"%s\"\n", name, msg);
    save_data(m, DATA_FILE);
}

static void cmd_title_set(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 2) {
        outmsg = "Fehler: 2 Argumente erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (argv[2][0] != '\"') {
        outmsg = "Fehler: Titel muss in Anführungszeichen stehen";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Fehler: Ungültige Gruppennummer";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    /* remove opening and closing quotes */
    char title[MAX_COMMAND_LENGTH];
    snprintf(title, sizeof(title), "%s", &argv[2][1]);
    int len = strlen(title) - 1;
    title[len] = '\0';

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    if (tox_group_set_title(m, groupnum, (uint8_t *) title, len) != 0) {
        outmsg = "Konnte den Titel nicht ändern. Das kann durch eine falsche Gruppennummer oder leere Gruppe ausgelöst werden";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        printf("%s konnte den Titel '%s' für Gruppe %d nicht ändern\n", name, title, groupnum);
        return;
    }

    int idx = group_index(groupnum);
    memcpy(Tox_Bot.g_chats[idx].title, title, len + 1);
    Tox_Bot.g_chats[idx].title_len = len;

    outmsg = "Gruppentitel geändert";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
    printf("%s ändert Gruppentitel der Gruppe %d zu %s\n", name, groupnum, title);
}


//------------------------------------------------------------------------------

static void cmd_register(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;


    if (argc < 2) {
        outmsg = "Fehler: 3 Argumente erforderlich";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (argv[1][0] != '\"') {
        outmsg = "Fehler: Name muss in Anführungszeichen stehen";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (argv[2][0] != '\"') {
        outmsg = "Fehler: ID muss in Anführungszeichen stehen";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    char name[MAX_COMMAND_LENGTH];
    snprintf(name, sizeof(name), "%s", &argv[1][1]);
    int len1 = strlen(name) - 1;
    name[len1] = '\0';

    char id[100];
    snprintf(id, sizeof(id), "%s", &argv[3][1]);
    int len = strlen(id) - 1;
    id[len] = '\0';

    FILE *out = fopen("contacts", "a");
    fprintf(out, "%s\t:\t%s\n", name, id);
    fclose(out);

    outmsg = "Registrierung erfolgreich";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
    printf("%s hat sich registriert.\n", name);
}

static void cmd_show_contacts(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    //Owner-File
    char owner[MAX_COMMAND_LENGTH + TOX_FRIEND_ADDRESS_SIZE + 30];
    int len = MAX_COMMAND_LENGTH + TOX_FRIEND_ADDRESS_SIZE + 30;
    char outmsg[len];

    FILE *in = fopen("contacts", "rb");
    char fInput[len + 2];

    snprintf(outmsg, sizeof(outmsg), "Kontakte\n");
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    if (in != NULL){
        int count = 0;
        while(fgets(fInput, (len), in) != NULL) {
            strncpy(outmsg, fInput, len);
            *strchr(outmsg, '\n') = ' ';
            tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
            count++;
        }
        fclose(in);
    } else {
        printf("Keine Einträge");
        strncpy(owner, "Keine Einträge", len);
        strncpy(outmsg, owner, len);
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        fclose(in);
    }
}


/* Parses input command and puts args into arg array.
   Returns number of arguments on success, -1 on failure. */
static int parse_command(const char *input, char (*args)[MAX_COMMAND_LENGTH])
{
    char *cmd = strdup(input);

    if (cmd == NULL)
        exit(EXIT_FAILURE);

    int num_args = 0;
    int i = 0;    /* index of last char in an argument */

    /* characters wrapped in double quotes count as one arg */
    while (num_args < MAX_NUM_ARGS) {
        int qt_ofst = 0;    /* set to 1 to offset index for quote char at end of arg */

        if (*cmd == '\"') {
            qt_ofst = 1;
            i = char_find(1, cmd, '\"');

            if (cmd[i] == '\0') {
                free(cmd);
                return -1;
            }
        } else {
            i = char_find(0, cmd, ' ');
        }

        memcpy(args[num_args], cmd, i + qt_ofst);
        args[num_args++][i + qt_ofst] = '\0';

        if (cmd[i] == '\0')    /* no more args */
            break;

        char tmp[MAX_COMMAND_LENGTH];
        snprintf(tmp, sizeof(tmp), "%s", &cmd[i + 1]);
        strcpy(cmd, tmp);    /* tmp will always fit inside cmd */
    }

    free(cmd);
    return num_args;
}

static struct {
    const char *name;
    void (*func)(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH]);
} commands[] = {
    { "default",          cmd_default       },
    { "group",            cmd_group         },
    { "gmessage",         cmd_gmessage      },
    { "hilfe",            cmd_help          },
    { "id",               cmd_id            },
    { "info",             cmd_info          },
    { "hallo",            cmd_invite        },
    { "leave",            cmd_leave         },
    { "master",           cmd_master        },
    { "name",             cmd_name          },
    { "passwd",           cmd_passwd        },
    { "purge",            cmd_purge         },
    { "status",           cmd_status        },
    { "statusmessage",    cmd_statusmessage },
    { "title",            cmd_title_set     },
    { "register",         cmd_register      },
    { "kontakte",         cmd_show_contacts },
    { NULL,               NULL              },
};

static int do_command(Tox *m, int friendnum, int num_args, char (*args)[MAX_COMMAND_LENGTH])
{
    int i;

    for (i = 0; commands[i].name; ++i) {
        if (strcmp(args[0], commands[i].name) == 0) {
            (commands[i].func)(m, friendnum, num_args - 1, args);
            return 0;
        }
    }

    return -1;
}

int execute(Tox *m, int friendnum, const char *input, int length)
{
    if (length >= MAX_COMMAND_LENGTH)
        return -1;

    char args[MAX_NUM_ARGS][MAX_COMMAND_LENGTH];
    int num_args = parse_command(input, args);

    if (num_args == -1)
        return -1;

    return do_command(m, friendnum, num_args, args);
}
