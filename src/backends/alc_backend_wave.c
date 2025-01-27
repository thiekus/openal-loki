/* -*- mode: C; tab-width:8; c-basic-offset:8 -*-
 * vi:set ts=8:
 *
 * waveout.c
 *
 * WAVE file output.  Context writes, we save and sleep.
 *
 */
#include "al_siteconfig.h"
#include "backends/alc_backend.h"
#include <stdlib.h>

#ifndef USE_BACKEND_WAVEOUT

void
alcBackendOpenWAVE_ (UNUSED(ALC_OpenMode mode), UNUSED(ALC_BackendOps **ops),
		     ALC_BackendPrivateData **privateData)
{
	*privateData = NULL;
}

#else

#include <AL/al.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "al_main.h"
#include "al_debug.h"
#include "audioconvert/ac_endian.h"

#define WAVEOUT_NAMELEN 16
#define HEADERSIZE      28
#define DATAADJUSTMENT  44


#ifdef WORDS_BIGENDIAN
#define RIFFMAGIC       0x52494646
#define WAVEMAGIC       0x57415645
#define FMTMAGIC        0x666D7420
#define DATAMAGIC	0x64617461
#else
#define RIFFMAGIC       0x46464952
#define WAVEMAGIC       0x45564157
#define FMTMAGIC        0x20746D66
#define DATAMAGIC	0x61746164
#endif /* WORDS_BIGENDIAN */

/* better wart needed? JIV FIXME */
#ifdef MAXNAMELEN
#undef MAXNAMELEN
#endif /* MAXNAMELEN */

#define MAXNAMELEN      1024

typedef struct waveout_s {
	FILE *fh;

	ALuint format;
	ALuint speed;
	ALuint channels;

	ALuint length;

	ALushort bitspersample;

	char name[WAVEOUT_NAMELEN];
	ALC_OpenMode mode;
} waveout_t;

static ALuint sleep_usec(ALuint speed, ALuint chunk);
static void apply_header(waveout_t *wave);
static const char *waveout_unique_name(char *template);

/*
 * convert_to_little_endian( ALuint bps, void *data, int nbytes )
 *
 * Convert data in place to little endian format.  NOP on little endian
 * machines.
 */
static void convert_to_little_endian( ALuint bps, void *data, int nbytes );

static void *grab_write_waveout(void) {
	FILE *fh;
	waveout_t *retval = NULL;
	char template[MAXNAMELEN] = "openal-";

	if(waveout_unique_name(template) == NULL) {
		perror("tmpnam");
	}

	fh = fopen(template, "w+b");
	if(fh == NULL) {
		fprintf(stderr,
			"waveout grab audio %s failed\n", template);
		return NULL;
	}

	retval = malloc(sizeof *retval);
	if(retval == NULL) {
		fclose(fh);
		return NULL;
	}

	memset(retval, 0, sizeof *retval);

	/* set to return params */
	retval->fh = fh;
	strncpy(retval->name, template, WAVEOUT_NAMELEN);

	retval->length = 0;
	retval->mode = ALC_OPEN_OUTPUT_;

	fprintf(stderr, "waveout grab audio %s\n", template);

	_alDebug(ALD_CONTEXT, __FILE__, __LINE__,
		"waveout grab audio ok");

	fseek(retval->fh, SEEK_SET, HEADERSIZE); /* leave room for header */
        return retval;
}

static void *grab_read_waveout(void) {
	return NULL;
}

static void waveout_blitbuffer(void *handle, const void *dataptr, int bytes_to_write) {
	waveout_t *whandle = NULL;

	if(handle == NULL) {
		return;
	}

	whandle = handle;

	if(whandle->fh == NULL) {
		return;
	}

	whandle->length += bytes_to_write;

	/*
	 * WAV files expect their PCM data in LE format.  If we are on
	 * big endian host, we need to convert the data in place.
	 * TODO: THIS BREAKS THE CONST CORRECTNESS!!!!!!!!!!!!!!!
	 *
	 */
	convert_to_little_endian( whandle->bitspersample,
				  (void *)dataptr,
				  bytes_to_write );

	fwrite(dataptr, 1, bytes_to_write, whandle->fh);

	_alMicroSleep(sleep_usec(whandle->speed, bytes_to_write));

        return;
}

/*
 *  close file, free data
 */
static void release_waveout(void *handle) {
	waveout_t *closer;

	if(handle == NULL) {
		return;
	}

	closer = handle;

	fprintf(stderr, "releasing waveout file %s\n",
		closer->name);

	fflush( closer->fh );
	apply_header( closer );

	fclose( closer->fh );
	free( closer );

	return;
}

static ALuint sleep_usec(ALuint speed, ALuint chunk) {
	ALuint retval;

	retval = 1000000.0 * chunk / speed;

#if 0
	fprintf(stderr,
		"(speed %d chunk %d retval = %d)\n",
		speed,
		chunk,
		retval);
#endif

	return retval;
}

/*
 *  FIXME: make endian correct
 */
static void apply_header(waveout_t *wave) {
	ALushort writer16;
	ALuint   writer32;

	/* go to beginning */
	if(fseek(wave->fh, SEEK_SET, 0) != 0) {
		fprintf(stderr,
			"Couldn't reset %s\n", wave->name);
	}

        /* 'RIFF' */
	writer32 = RIFFMAGIC;
	fwrite(&writer32, 1, sizeof writer32, wave->fh);

	/* total length */
	writer32 = swap32le(wave->length);
	fwrite(&writer32, 1, sizeof writer32, wave->fh);

        /* 'WAVE' */
	writer32 = WAVEMAGIC;
	fwrite(&writer32, 1, sizeof writer32, wave->fh);

        /* 'fmt ' */
	writer32 = FMTMAGIC;
	fwrite(&writer32, 1, sizeof writer32, wave->fh);

        /* fmt chunk length */
	writer32 = swap32le(16);
	fwrite(&writer32, 1, sizeof writer32, wave->fh);

        /* ALushort encoding */
	writer16 = swap16le(1);
	fwrite(&writer16, 1, sizeof writer16, wave->fh);

	/* Alushort channels */
	writer16 = swap16le(wave->channels);
	fwrite(&writer16, 1, sizeof writer16, wave->fh);

	/* ALuint frequency */
	writer32 = swap32le(wave->speed);
	fwrite(&writer32, 1, sizeof writer32, wave->fh);

	/* ALuint byterate  */
	writer32 = swap32le(wave->speed / sizeof (ALshort)); /* FIXME */
	fwrite(&writer32, 1, sizeof writer32, wave->fh);

	/* ALushort blockalign */
	writer16 = 0;
	fwrite(&writer16, 1, sizeof writer16, wave->fh);

	/* ALushort bitspersample */
	writer16 = swap16le(wave->bitspersample);
	fwrite(&writer16, 1, sizeof writer16, wave->fh);

        /* 'data' */
	writer32 = DATAMAGIC;
	fwrite(&writer32, 1, sizeof writer32, wave->fh);

	/* data length */
	writer32 = swap32le(wave->length - DATAADJUSTMENT); /* samples */
	fwrite(&writer32, 1, sizeof writer32, wave->fh);

	fprintf(stderr, "waveout length %d\n", wave->length);

	return;
}

static const char *waveout_unique_name(char *template) {
	static char retval[MAXNAMELEN];
	int template_offset;
	static int sequence = 0;
	struct stat buf;

	strncpy(retval, template, MAXNAMELEN - 2);
	retval[MAXNAMELEN - 1] = '\0';

	template_offset = strlen(retval);

	if(template_offset >= MAXNAMELEN - 28) { /* kludgey */
		/* template too big */
		return NULL;
	}

	do {
		/* repeat until we have a unique name */
		snprintf(&retval[template_offset], sizeof(retval) - template_offset, "%d.wav", sequence++);
		strncpy(template, retval, MAXNAMELEN);
	} while(stat(retval, &buf) == 0);

	return retval;
}

static ALboolean set_write_waveout(void *handle,
				   UNUSED(ALuint *bufsiz),
				   ALenum *fmt,
				   ALuint *speed) {
	waveout_t *whandle;
	ALuint chans = _alGetChannelsFromFormat( *fmt );

	if(handle == NULL) {
		return AL_FALSE;
	}

	whandle = handle;

	whandle->speed    = *speed;
	whandle->format   = *fmt;
	whandle->channels = chans;
	whandle->bitspersample = _alGetBitsFromFormat(*fmt);

        return AL_TRUE;
}

static ALboolean set_read_waveout(UNUSED(void *handle),
				  UNUSED(ALuint *bufsiz),
				  UNUSED(ALenum *fmt),
				  UNUSED(ALuint *speed)) {

	return AL_FALSE;
}

static ALboolean
alcBackendSetAttributesWAVE_(void *handle, ALuint *bufsiz, ALenum *fmt, ALuint *speed)
{
	return ((waveout_t *)handle)->mode == ALC_OPEN_INPUT_ ?
		set_read_waveout(handle, bufsiz, fmt, speed) :
		set_write_waveout(handle, bufsiz, fmt, speed);
}

/*
 * convert_to_little_endian( ALuint bps, void *data, int nbytes )
 *
 * Convert data in place to little endian format.  bps is the bits per
 * sample in data, nbytes is the length of data in bytes.  If bps is 8,
 * or the machine is little endian, this is a nop.
 *
 * FIXME:
 *	We only handle 16-bit signed formats for now.  Should fix this.
 */
static void convert_to_little_endian( ALuint bps, void *data, int nbytes )
{
	assert( data );
	assert( nbytes > 0 );

	if( bps == 8 ) {
		/* 8-bit samples don't need to be converted */
		return;
	}

	assert( bps == 16 );

#ifdef WORDS_BIGENDIAN
	/* do the conversion */
	{
		ALshort *outp = data;
		ALuint i;
		for( i = 0; i < nbytes/sizeof(ALshort); i++ ) {
			outp[i] = swap16le( outp[i] );
		}
	}
#else
	(void)data; (void)nbytes;
#endif /* WORDS_BIG_ENDIAN */
}

static void
pause_waveout( UNUSED(void *handle) )
{
}

static void
resume_waveout( UNUSED(void *handle) )
{
}

static ALsizei
capture_waveout( UNUSED(void *handle), UNUSED(void *capture_buffer), UNUSED(int bufsiz) )
{
	return 0;
}

static ALfloat
get_waveoutchannel( UNUSED(void *handle), UNUSED(ALuint channel) )
{
	return 0.0;
}

static int
set_waveoutchannel( UNUSED(void *handle), UNUSED(ALuint channel), UNUSED(ALfloat volume) )
{
	return 0;
}

static ALC_BackendOps waveOps = {
	release_waveout,
	pause_waveout,
	resume_waveout,
	alcBackendSetAttributesWAVE_,
	waveout_blitbuffer,
	capture_waveout,
	get_waveoutchannel,
	set_waveoutchannel
};

void
alcBackendOpenWAVE_ (ALC_OpenMode mode, ALC_BackendOps **ops, ALC_BackendPrivateData **privateData)
{
	*privateData = (mode == ALC_OPEN_INPUT_) ? grab_read_waveout() : grab_write_waveout();
	if (*privateData != NULL) {
		*ops = &waveOps;
	}
}

#endif /* USE_BACKEND_WAVEOUT */
