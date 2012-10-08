#include <stdio.h>
#include <string.h>
#include <ev.h>
#include <stdlib.h>
#include <time.h>
#include <alsa/asoundlib.h>

#include "volcon_config.h"
#include "ami.h"
#include "debug.h"

#define CON_DEBUG
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
		(ami->authenticated) ? "Disconnected from" : "Can't connect to",
		ami_getvar(event, "Host"),
		ami_getvar(event, "IP"),
		ami_getvar(event, "Port"),
		ami_getvar(event, "Reason"));
	exit(0);
}

void event_connect (ami_event_t *event) {
	printf("* Connected to %s(%s:%s)\n",
		ami_getvar(event, "Host"),
		ami_getvar(event, "IP"),
		ami_getvar(event, "Port"));
}

int setvolume (int volume, int *prev_volume) {
	long min, max;
	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;
	const char *card = CONFIG_CARD;
	const char *selem_name = CONFIG_SELEM;

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, card);
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, selem_name);
	snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

	// ha meg van adva prev_volume, akkor beleírjuk a jelenlegi hangerőt
	if (prev_volume != NULL) {
		snd_mixer_selem_get_playback_volume(elem, 0, (long*)prev_volume);
		*prev_volume -= min;
		max -= min;
		min = 0;
		*prev_volume = 100 * (*prev_volume) / max; // make the value bound from 0 to 100
		*prev_volume = *prev_volume + 1;
		if (*prev_volume > 100)
			*prev_volume = 100;
	}

	// ha a volume != -1 akkor beállítjuk a hangerőt
	if (volume >= 0) {
		long setvol;
		snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
		setvol = volume * max / 100;
		snd_mixer_selem_set_playback_volume_all(elem, setvol);
	}

	snd_mixer_close(handle);
	return 0;
}

int getvolume () {
	int volume;
	setvolume(-1, &volume);
	return volume;
}

/**
 *
 * name: volume_reel
 * @param vol_to ahova a hangerőt le vagy fel kell tekerni
 * @param steps a hangerő tekerése ennyi lépésben történjen
 * @param fade_time a hangerő tekerése összesen ennyi ideig tartson (milliszekundum)
 * @return nothing
 */
void volume_reel (int vol_to, int steps, int fade_time) {
	int i;

	int vol_from;
	setvolume(-1, &vol_from);

	// ha a jelenlegi és a beállítani kívánt hangerő ugyan az, akkor return
	if (vol_to == vol_from)
		return;

	int diff = abs(vol_from - vol_to);
	float uni = (float)diff / (float)steps;

debi(vol_from); debi(vol_to); debi(diff); debf(uni);
	int udelay = fade_time * 1000 / steps;
	for (i = 0; i < steps; i++) {
		int ch = (uni * i) * ((vol_from < vol_to) ? 1 : -1);
		int destvol = vol_from + ch;
		debi(ch); debi(destvol);
		setvolume(destvol, NULL);
		usleep(udelay);
	}
	setvolume(vol_to, NULL);

}

void event_extensionstatus (ami_event_t *event) {
	char *exten = ami_getvar(event, "Exten");
	int status = atoi(ami_getvar(event, "Status"));


	// csak akkor megyünk tovább, ha a CONFIG_EXTEN mellékről van szó
	if (strcmp(exten, CONFIG_EXTEN))
		return;

	con_debug("-- Exten: %s Status = %d", exten, status);

	printf("volume %d\n", getvolume());
	//~ int volume;
	//~ volumt = getvolume();

	switch (status) {
		// Kagyló letesz, hangerő felhangosít
		case 0: // Down
			if (volume_state == HIGH) {
				volume_state = LOW;
				//~ volume_low = volume;
				//~ if (volume_high >= 0)
					volume_reel(volume_high, 20, 3000);
			}
			return;

		// Kagyló felvesz, hangerő lehalkít
		case 1: // Up (?)
		case 8: // Ring (?)
			if (volume_state == LOW) {
				volume_state = HIGH;
				//~ volume_high = getvolume();
				//~ if (volume_low >= 0)
					volume_reel(volume_low, 10, 1000);
			}
			return;
	}

	debs("hello");

}

int main (int argc, char *argv[]) {
	ami = ami_new(EV_DEFAULT);
	if (ami == NULL) {
		con_debug("ami_new() returned NULL");
		return 1;
	}

	if (argc < 2) {
		printf("JSS AMI logger (c) 2012 JSS&Hayer - http://libamievent.jss.hu\n"
		       "usage: %s <host>\n", argv[0]);
		exit(1);
	}

	host = argv[1];

	printf("* Connecting to %s ...\n", host);
	ami_credentials(ami, "jsi", "pwd", host, "5038");

	ami_event_register(ami, event_disconnect, NULL, "Disconnect");
	ami_event_register(ami, event_connect, NULL, "Connect");
	ami_event_register(ami, event_extensionstatus, NULL, "Event: ExtensionStatus");

	ami_connect(ami);

	//~ long volume = atol(argv[2]);
	//~ int prev_volume;
	//~ setvolume(volume, &prev_volume);
	//~ volume_reel(volume, 20, 1000);
	//~ printf("previous volume = %i\n", prev_volume);

	volume_state = LOW; // feltételezzük, hogy indításnál a kagyló le van téve
	volume_high = getvolume(); // feltételezzük, hogy a zene is hangos:)

	ev_loop(EV_DEFAULT, 0);

	ami_destroy(ami);
	return 0;
}



