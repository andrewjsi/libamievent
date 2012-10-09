#include <stdio.h>
#include <string.h>
#include <ev.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "volcon_config.h"
#include "ami.h"
#include "debug.h"

//~ #define CON_DEBUG
#include "logger.h"

ev_timer timer;
ev_timer timer_reconnect;

char *host;
ami_t *ami;
int volume_high = 90;
int volume_low = 30;

enum {
	HIGH,
	LOW,
} volume_state;

void event_disconnect (ami_event_t *event) {
	ami_t *ami = event->ami;
	printf("* %s %s(%s:%s): %s\n",
		(!strcmp(ami_getvar(event, "WasAuthenticated"), "1")) ? "Disconnected from" : "Can't connect to",
		ami_getvar(event, "Host"),
		ami_getvar(event, "IP"),
		ami_getvar(event, "Port"),
		ami_getvar(event, "Reason"));
	ami_connect_delayed(ami, 3000);
}

void event_connect (ami_event_t *event) {
	printf("* Connected to %s(%s:%s)\n",
		ami_getvar(event, "Host"),
		ami_getvar(event, "IP"),
		ami_getvar(event, "Port"));
}

int snd_get_card_number (char *name) {
	char cmd[128];
	char out[128];

	#define CARDS "/proc/asound/cards"
	if (access(CARDS, R_OK) < 0) {
		printf("Can't open %s\n", CARDS);
		return -1;
	}

	FILE *f;
	snprintf(cmd, sizeof(cmd), "cat %s |grep -w \\\\[%s | awk {'print $1'}", CARDS, name);
	f = popen(cmd, "r");
	if (f == NULL)
		return -1;

	int card_number = -1;
	if (fgets(out, sizeof(out), f)) {
		card_number = atoi(out);
//~ debs(out);
	}

	pclose(f);
//~ debs(cmd);
	return card_number;
}

// visszaadja %-ban a CONFIG_CARD hangkártya CONFIG_SELEM vezérlő playback hangerejét
// hiba esetén -1
int snd_getvolume () {
	char cmd[128];
	char out[128];

	FILE *f;
	snprintf(
		cmd,
		sizeof(cmd),
		"amixer -c %d sget %s playback |tail -n 1 |cut -d '[' -f2 |cut -d '%%' -f1",
		snd_get_card_number(CONFIG_CARD),
		CONFIG_SELEM
	);
//~ debs(cmd);

	f = popen(cmd, "r");
	if (f == NULL) {
		perror("popen");
		return -1;
	}

	int volume = -1;
	if (fgets(out, sizeof(out), f)) {
		volume = atoi(out);
//~ debs(out);
	}

	pclose(f);
	con_debug("volume get: %d%%", volume);
	return volume;
}

void snd_setvolume (int volume) {
	char cmd[128];
	snprintf(
		cmd,
		sizeof(cmd),
		"amixer -q -c %d sset %s playback %d%%",
		snd_get_card_number(CONFIG_CARD),
		CONFIG_SELEM,
		volume
	);
//~ debs(cmd);

	con_debug("volume set: %d%%", volume);
	system(cmd);
}

/**
 *
 * @name: volume_reel
 * @param vol_to ahova a hangerőt le vagy fel kell tekerni
 * @param steps a hangerő tekerése ennyi lépésben történjen
 * @param fade_time a hangerő tekerése összesen ennyi ideig tartson (milliszekundum)
 * @return nothing
 */
void volume_reel (int vol_to, int steps, int fade_time) {
	int i;

	int vol_from = snd_getvolume();

	printf("* Reeling volume %d%% -> %d%%\n", vol_from, vol_to);

	// ha a jelenlegi és a beállítani kívánt hangerő ugyan az, akkor return
	if (vol_to == vol_from)
		return;

	int diff = abs(vol_from - vol_to);
	float uni = (float)diff / (float)steps;

//~ debi(vol_from); debi(vol_to); debi(diff); debf(uni);
	int udelay = fade_time * 1000 / steps;
	for (i = 0; i < steps; i++) {
		int ch = (uni * i) * ((vol_from < vol_to) ? 1 : -1);
		int destvol = vol_from + ch;
//~ debi(ch); debi(destvol);
		snd_setvolume(destvol);
		usleep(udelay);

		/* ha az usleep() kozben egy masik program modositotta a hangerot,
		akkor azonnal visszaterunk. Tehat, ha a volume_reel() epp emeli a
		hangerot, de a user egy masik hangeroszabalyzon hallkit egy kicsit,
		akkor a volume_reel() azonnal visszalep es nem eroszakoskodik a
		userrel. */
		if (destvol != snd_getvolume())
			return;
	}
	snd_setvolume(vol_to);

}

static void event_extensionstatus (ami_event_t *event) {
	char *exten = ami_getvar(event, "Exten");
	int status = atoi(ami_getvar(event, "Status"));

	// csak akkor megyünk tovább, ha a CONFIG_EXTEN mellékről van szó
	if (strcmp(exten, CONFIG_EXTEN))
		return;

	int current_volume = snd_getvolume();

	switch (status) {
		// Kagyló letesz, hangerő felhangosít
		case 0: // Down
			if (volume_state == HIGH) {
				volume_state = LOW;

				volume_low = snd_getvolume();

				// ha a jelenlegi hangerő hangosabb, mint ahová felhangosítanánk a hangerőt
				if (current_volume >= volume_high)
					return;

				if (volume_high >= 0)
					volume_reel(volume_high, 20, 3000);
			}
			return;

		// Kagyló felvesz, hangerő lehalkít
		case 1: // Up (?)
		case 8: // Ring (?)
			if (volume_state == LOW) {
				volume_state = HIGH;

				volume_high = current_volume;

				// ha a jelenlegi hangerő hallkabb, mint ahová lehallkítanánk a hangerőt
				if (current_volume <= volume_low)
					return;


				if (volume_low >= 0)
					volume_reel(volume_low, 10, 1000);
			}
			return;
	}
}

int main (int argc, char *argv[]) {
	ami = ami_new(EV_DEFAULT);
	if (ami == NULL) {
		con_debug("ami_new() returned NULL");
		return 1;
	}

	if (argc < 2) {
		printf("JSS VolCon for Asterisk (c) 2012 JSS&Hayer - http://libamievent.jss.hu\n"
		       "usage: %s <host>\n", argv[0]);
		exit(1);
	}

	host = argv[1];

	printf("* ALSA card name: %s\n", CONFIG_CARD);
	printf("* Mixer controller name: %s\n", CONFIG_SELEM);
	printf("* Watching extension %s ...\n", CONFIG_EXTEN);

	volume_state = LOW; // feltételezzük, hogy indításnál a kagyló le van téve
	volume_high = snd_getvolume(); // ezért a mostani hangerőt, mint hangos (high) hangerőt memorizáljuk
	if (volume_high < 0) {
		printf("ERROR fetching current volume...\n");
		return 1;
	} else {
		printf("* Current volume: %d%%\n", volume_high);
	}

	printf("* Connecting to %s ...\n", host);
	ami_credentials(ami, "jsi", "pwd", host, "5038");
	ami_event_register(ami, event_disconnect, NULL, "Disconnect");
	ami_event_register(ami, event_connect, NULL, "Connect");
	ami_event_register(ami, event_extensionstatus, NULL, "Event: ExtensionStatus");
	ami_connect(ami);

	ev_loop(EV_DEFAULT, 0);

	ami_destroy(ami);
	return 0;
}



