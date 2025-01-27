/* -*- mode: C; tab-width:8; c-basic-offset:8 -*-
 * vi:set ts=8:
 *  alsa.c

   ALSA backend for OpenAL

    Dirk Ehlke
    EMail: dehlke@mip.informatik.uni-kiel.de

    Multimedia Information Processing
    Christian-Albrechts-University of Kiel

    2002/06/13
*/


#ifndef _SVID_SOURCE
#define _SVID_SOURCE
#endif /* _SVID_SOURCE */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */

#include "al_siteconfig.h"
#include <stdlib.h>
#include "backends/alc_backend.h"

#ifndef USE_BACKEND_ALSA

void alcBackendOpenALSA_ (UNUSED(ALC_OpenMode mode), UNUSED(ALC_BackendOps **ops),
			  ALC_BackendPrivateData **privateData)
{
	*privateData = NULL;
}

#else

#include <AL/alext.h>
#include <stdio.h>

#include "al_config.h"
#include "al_debug.h"
#include "al_main.h"

#ifdef OPENAL_DLOPEN_ALSA
#include <dlfcn.h>
#endif

#include <alsa/asoundlib.h>

static void *alsa_lib_handle = NULL;
static int (*psnd_pcm_hw_params_malloc)(snd_pcm_hw_params_t **ptr) = NULL;
static void (*psnd_pcm_hw_params_free)(snd_pcm_hw_params_t *obj) = NULL;
static const char *(*psnd_strerror)(int errnum) = NULL;
static size_t (*psnd_pcm_info_sizeof)(void) = NULL;
static int (*psnd_pcm_close)(snd_pcm_t *pcm) = NULL;
static int (*psnd_pcm_hw_params)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params) = NULL;
static int (*psnd_pcm_hw_params_any)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params) = NULL;
static int (*psnd_pcm_hw_params_set_access)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t acc) = NULL;
static int (*psnd_pcm_hw_params_set_buffer_size_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val) = NULL;
static int (*psnd_pcm_hw_params_set_channels)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val) = NULL;
static int (*psnd_pcm_hw_params_set_format)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val) = NULL;
static int (*psnd_pcm_hw_params_set_periods_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir) = NULL;
static int (*psnd_pcm_hw_params_set_rate_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir) = NULL;
static size_t (*psnd_pcm_hw_params_sizeof)(void) = NULL;
static int (*psnd_pcm_prepare)(snd_pcm_t *pcm) = NULL;
static snd_pcm_sframes_t (*psnd_pcm_readi)(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size) = NULL;
static int (*psnd_pcm_resume)(snd_pcm_t *pcm) = NULL;
static snd_pcm_sframes_t (*psnd_pcm_writei)(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size) = NULL;
static int (*psnd_pcm_hw_params_set_period_size_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir) = NULL;
static int (*psnd_pcm_open)(snd_pcm_t **pcm, const char *name,
                     snd_pcm_stream_t stream, int mode) = NULL;
static int (*psnd_pcm_nonblock)(snd_pcm_t * pcm, int nonblock) = NULL;

/* !!! FIXME: hhm...this is a problem. */
#if (SND_LIB_MAJOR == 0)
static int (*psnd_pcm_hw_params_get_buffer_size)(const snd_pcm_hw_params_t *params) = NULL;
static int (*psnd_pcm_hw_params_get_channels)(const snd_pcm_hw_params_t *params) = NULL;
static snd_pcm_sframes_t (*psnd_pcm_hw_params_get_period_size)(const snd_pcm_hw_params_t *params, int *dir) = NULL;
#else
static int (*psnd_pcm_hw_params_get_buffer_size)(const snd_pcm_hw_params_t *params,
                                          snd_pcm_uframes_t *val) = NULL;
static int (*psnd_pcm_hw_params_get_channels)(const snd_pcm_hw_params_t *params, unsigned int *val) = NULL;
static int (*psnd_pcm_hw_params_get_period_size)(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir) = NULL;
#endif

static int openal_load_alsa_library(void)
{
#ifdef OPENAL_DLOPEN_ALSA
        char * error = NULL;
#endif
    
	if (alsa_lib_handle != NULL)
		return 1;  /* already loaded. */

        /* versioned symbol fetching macro, or NULL if no dlvsym available */
#ifdef _GNU_SOURCE
#define AL_DLVSYM dlvsym
#else
#define AL_DLVSYM(a,b,c) NULL
#endif

        /* ALSA uses symbol versioning, which is usually a good thing except
           that it turns dlsym() into a lottery.  So, we look for known-good
           symbol versions first before falling back to unversioned symbols. */
        /* this is our current preferred symbol versioning order:
           ALSA_0.9.0rc4 > (generic) */
	#ifdef OPENAL_DLOPEN_ALSA
		#define OPENAL_LOAD_ALSA_SYMBOL(x) p##x = AL_DLVSYM(alsa_lib_handle, #x, "ALSA_0.9.0rc4"); \
                                                   error = dlerror(); \
                                                   if ((error != NULL)||(p##x == NULL)) { \
                                                           p##x = dlsym(alsa_lib_handle, #x); \
                                                           error = dlerror(); \
                                                           if ((error != NULL)||(p##x == NULL)) { \
                                                                   fprintf(stderr,"Could not resolve ALSA symbol %s: %s\n", #x, ((error!=NULL)?(error):("(null)"))); \
                                                                   dlclose(alsa_lib_handle); alsa_lib_handle = NULL; \
                                                                   return 0; } else _alDebug(ALD_MAXIMUS, __FILE__, __LINE__, "got %s", #x); \
                                                   } else _alDebug(ALD_MAXIMUS, __FILE__, __LINE__, "got %s", #x "@ALSA_0.9rc4");
                dlerror(); /* clear error state */
		alsa_lib_handle = dlopen("libasound.so.2", RTLD_LAZY | RTLD_GLOBAL);
                error = dlerror();
		if (alsa_lib_handle == NULL) {
                        fprintf(stderr,"Could not open ALSA library: %s\n",((error!=NULL)?(error):("(null)")));
			return 0;
                }
	#else
		#define OPENAL_LOAD_ALSA_SYMBOL(x) p##x = x;
		alsa_lib_handle = (void *) 0xF00DF00D;
	#endif

	OPENAL_LOAD_ALSA_SYMBOL(snd_strerror);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_info_sizeof);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_malloc);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_free);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_open);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_close);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_any);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_get_buffer_size);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_get_channels);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_get_period_size);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_set_access);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_set_buffer_size_near);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_set_channels);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_set_format);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_set_period_size_near);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_set_periods_near);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_set_rate_near);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_hw_params_sizeof);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_open);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_nonblock);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_prepare);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_readi);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_resume);
	OPENAL_LOAD_ALSA_SYMBOL(snd_pcm_writei);

	return 1;
}

/* alsa stuff */
#define DEFAULT_DEVICE "plughw:0,0"

/* convert from AL to ALSA format */
static int AL2ALSAFMT(ALenum format);
static int FRAMESIZE(snd_pcm_format_t fmt, unsigned int chans);


/*
 * get either the default device name or something the
 * user specified
 */
static void get_in_device_name(char *retref, size_t retsize);
static void get_out_device_name(char *retref, size_t retsize);

struct alsa_info
{
	snd_pcm_t *handle;
	snd_pcm_format_t format;
	unsigned int speed;
	unsigned int channels;
	unsigned int framesize;
	unsigned int periods;
	snd_pcm_uframes_t bufframesize;
	fd_set fd_set;
	int setup_read, setup_write;
	ALC_OpenMode mode;
};

static void release_alsa(void *handle)
{
	struct alsa_info *ai = handle;
	if(handle == NULL)
	  return;
	psnd_pcm_close(ai->handle);
	free(ai);
}

static void *grab_read_alsa( void )
{
	struct alsa_info *retval;
	snd_pcm_t *handle;
	char card_name[256];
	int err;

	if (!openal_load_alsa_library())
		return NULL;

	get_in_device_name(card_name, 256);

	err = psnd_pcm_open(&handle, card_name, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
	if(err < 0)
	{
		const char *serr = psnd_strerror(err);

		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"grab_alsa: init failed: %s", serr);

		return NULL;
	}

	retval = malloc(sizeof *retval);
	retval->handle      = handle;
	retval->format      = 0;
	retval->channels    = 0;
	retval->speed       = 0;
	retval->framesize   = 0;
	retval->bufframesize= 0;
	retval->periods     = 0;
	retval->setup_read	= 0;
	retval->setup_write	= 0;
	retval->mode = ALC_OPEN_INPUT_;

	_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
			"grab_alsa: init ok, using %s", card_name);

	return retval;
}

static void get_out_device_name(char *retref, size_t retsize)
{
	Rcvar rcv;

	assert(retref);

	if (!(rcv = rc_lookup("alsa-device")))
		rcv = rc_lookup("alsa-out-device");
	if (rcv != NULL)
	{
		if(rc_type(rcv) == ALRC_STRING)
		{
			rc_tostr0(rcv, retref, retsize);
                        return;

		}
	}

	strncpy(retref, DEFAULT_DEVICE, retsize);
	retref[retsize - 1] = '\0';
}

static void get_in_device_name(char *retref, size_t retsize)
{
	Rcvar rcv;

	assert(retref);

	if (!(rcv = rc_lookup("alsa-in-device")))
		rcv = rc_lookup("alsa-device");
	if (rcv != NULL)
	{
		if(rc_type(rcv) == ALRC_STRING)
		{
			rc_tostr0(rcv, retref, retsize);
                        return;

		}
	}

	strncpy(retref, DEFAULT_DEVICE, retsize);
	retref[retsize - 1] = '\0';
}

static void *grab_write_alsa( void )
{
	struct alsa_info *retval;
	snd_pcm_t *handle;
	char card_name[256];
	int err;

	if (!openal_load_alsa_library())
		return NULL;

	get_out_device_name(card_name, 256);

        /* Try to open the device without blocking, so we can 
         * try other backends even if this would block.
         */
	err = psnd_pcm_open(&handle, card_name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if(err < 0)
	{
		const char *serr = psnd_strerror(err);

		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
			"grab_alsa: init failed: %s", serr);

		return NULL;
	}

        /* Now that we have successfully opened the device, 
         * we can put it into blocking mode.
         */
        err = psnd_pcm_nonblock(handle,0);
	if(err < 0)
	{
		const char *serr = psnd_strerror(err);

		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
			"grab_alsa: could not put device into blocking mode: %s", serr);
	}
        
	retval = malloc(sizeof *retval);
	retval->handle      = handle;
	retval->format      = 0;
	retval->channels    = 0;
	retval->speed       = 0;
	retval->framesize   = 0;
	retval->bufframesize= 0;
	retval->periods     = 0;
	retval->setup_read	= 0;
	retval->setup_write	= 0;
	retval->mode = ALC_OPEN_OUTPUT_;

	_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
		 "grab_alsa: init ok, using %s", card_name);

	return retval;
}

static ALboolean set_read_alsa( void *handle,
				ALuint *bufsiz,
				ALenum *fmt,
				ALuint *speed)
{
	struct alsa_info *ai = handle;
	snd_pcm_hw_params_t *setup;
	snd_pcm_uframes_t buffer_size, period_size;
	snd_pcm_t *phandle = 0;
	int err, dir;

	if( (ai == NULL) || (ai->handle == NULL) )
		return AL_FALSE;


	if ((*fmt == AL_FORMAT_QUAD8_LOKI) || (*fmt == AL_FORMAT_STEREO8))
		*fmt = AL_FORMAT_MONO8;
	if ((*fmt == AL_FORMAT_QUAD16_LOKI) || (*fmt == AL_FORMAT_STEREO16))
		*fmt = AL_FORMAT_MONO16;

	ai->channels    = 1;
	ai->format      = (unsigned int) AL2ALSAFMT(*fmt);
	ai->speed       = (unsigned int) *speed;
	ai->framesize   = (unsigned int) FRAMESIZE(ai->format, ai->channels);
	ai->bufframesize= (snd_pcm_uframes_t) 8192;
	ai->periods     = 2;

	_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
			"alsa info (read): channels: %u, format: %u, speed: %u, framesize: %u, bufframesize: %lu,periods: %u",
			ai->channels, ai->format, ai->speed, ai->framesize, ai->bufframesize, ai->periods);

	phandle = ai->handle;

	psnd_pcm_hw_params_malloc(&setup);
	err = psnd_pcm_hw_params_any(phandle, setup);
	if(err < 0)
	{
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_read_alsa: Could not query parameters: %s",psnd_strerror(err));

		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

	/* set the interleaved read format */
	err = psnd_pcm_hw_params_set_access(phandle, setup, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_read_alsa: Could not set access type: %s",psnd_strerror(err));
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

	/* set format */
	err = psnd_pcm_hw_params_set_format(phandle, setup, ai->format);
	if(err < 0)
	{
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_read_alsa: could not set format: %s",psnd_strerror(err));

		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}


	/* channels */
	err = psnd_pcm_hw_params_set_channels(phandle, setup, ai->channels);
	if(err < 0)
	{
		unsigned int ch;
#if (SND_LIB_MAJOR == 0)
		ch = err = psnd_pcm_hw_params_get_channels(setup);
#else
		err = psnd_pcm_hw_params_get_channels(setup, &ch);
#endif

		if (ch != ai->channels) {
			_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
					"set_read_alsa: could not set channels: %s",psnd_strerror(err));

			psnd_pcm_hw_params_free(setup);
			return AL_FALSE;
		}
	}


	/* sampling rate */
	err = psnd_pcm_hw_params_set_rate_near(phandle, setup, &ai->speed, NULL);
	if(err < 0)
	{
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_read_alsa: could not set speed: %s",psnd_strerror(err));

		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

	/* Set number of periods. Periods used to be called fragments. */
	{ snd_pcm_uframes_t val = 4096;
	err = psnd_pcm_hw_params_set_period_size_near(phandle, setup, &val, NULL);
	if (err < 0) {
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_read_alsa: %s", psnd_strerror(err));
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}
	}

	err = psnd_pcm_hw_params_set_periods_near(phandle, setup, &ai->periods, 0);
	if (err < 0) {
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_read_alsa: %s", psnd_strerror(err));
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

	err = psnd_pcm_hw_params_set_buffer_size_near(phandle, setup, &ai->bufframesize);
	if (err < 0) {
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_read_alsa: %s, size: %lu, speed: %d",
				psnd_strerror(err), ai->bufframesize, ai->speed);
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

#if (SND_LIB_MAJOR == 0)
	buffer_size = psnd_pcm_hw_params_get_buffer_size(setup);
	period_size = psnd_pcm_hw_params_get_period_size(setup, &dir);
#else
	psnd_pcm_hw_params_get_buffer_size(setup, &buffer_size);
	psnd_pcm_hw_params_get_period_size(setup, &period_size, &dir);
#endif

	_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
			"set_read_alsa (info): Buffersize = %lu (%u)",buffer_size, *bufsiz);
	_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
			"set_read_alsa (info): Periodsize = %lu", period_size);
	*bufsiz = buffer_size * ai->framesize;

	err = psnd_pcm_hw_params(phandle, setup);
	if(err < 0)
	{
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_read_alsa: %s", psnd_strerror(err));
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

	err = psnd_pcm_prepare(phandle);
	if(err < 0)
	{
		_alDebug(ALD_MAXIMUS,  __FILE__, __LINE__,
				"set_read_alsa %s", psnd_strerror(err));
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

	_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
		 "set_read_alsa: handle: %p, phandle: %p", handle, (void*)phandle);
	ai->setup_read = 1;

	psnd_pcm_hw_params_free(setup);
	return AL_TRUE;
}

static ALboolean set_write_alsa(void *handle,
				ALuint *bufsiz,
				ALenum *fmt,
				ALuint *speed)
{
	struct alsa_info *ai = handle;
	snd_pcm_hw_params_t *setup;
	snd_pcm_uframes_t buffer_size, period_size;
	snd_pcm_t *phandle = 0;
	int err, dir;

	if( (ai == NULL) || (ai->handle == NULL) )
		return AL_FALSE;

	ai->channels    = (unsigned int) _alGetChannelsFromFormat(*fmt);
	ai->format      = (unsigned int) AL2ALSAFMT(*fmt);
	ai->speed       = (unsigned int) *speed;
	ai->framesize   = (unsigned int) FRAMESIZE(ai->format, ai->channels);
	ai->bufframesize= (snd_pcm_uframes_t) *bufsiz / ai->framesize * 4;
	ai->periods     = 2;

	_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
			"alsa info (write): channels: %u, format: %u, speed: %u, framesize: %u, bufframesize: %lu, periods: %u",
			ai->channels, ai->format, ai->speed, ai->framesize, ai->bufframesize, ai->periods);

	phandle = ai->handle;

	psnd_pcm_hw_params_malloc(&setup);
	err = psnd_pcm_hw_params_any(phandle, setup);
	if(err < 0)
	{
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_write_alsa: Could not query parameters: %s",psnd_strerror(err));

		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

	/* set the interleaved write format */
	err = psnd_pcm_hw_params_set_access(phandle, setup, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_write_alsa: Could not set access type: %s",psnd_strerror(err));
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

	/* set format */
	err = psnd_pcm_hw_params_set_format(phandle, setup, ai->format);
	if(err < 0)
	{
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_write_alsa: could not set format: %s",psnd_strerror(err));

		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}


	/* channels */
	err = psnd_pcm_hw_params_set_channels(phandle, setup, ai->channels);
	if(err < 0)
	{
		unsigned int ch;
#if (SND_LIB_MAJOR == 0)
		ch = err = psnd_pcm_hw_params_get_channels(setup);
#else
		err = psnd_pcm_hw_params_get_channels(setup, &ch);
#endif

		if (ch != ai->channels) {
			_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
					"set_write_alsa: could not set channels: %s",psnd_strerror(err));

			psnd_pcm_hw_params_free(setup);
			return AL_FALSE;
		}
	}


	/* sampling rate */
	err = psnd_pcm_hw_params_set_rate_near(phandle, setup, &ai->speed,
                                               NULL);
	if(err < 0) {
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
                         "set_write_alsa: could not set speed: %s",
                         psnd_strerror(err));
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	} else if (err > 0) {
                /* sampling rate is in 'err' */
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
                         "set_write_alsa: alsa speed returned is %u rather than %u",
                         (unsigned int) err, ai->speed);
		ai->speed = (unsigned int) err;
                if (ai->speed > 200000) {
                        /* This can happen, and is the precursor to other Bad
                           Things.  It usually means we dlsym()d a crappy
                           interface version instead of known-good one, and
                           our list of preferential interfaces may need
                           updating. */
                        _alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
                                 "set_write_alsa: hw speed %u not sane.  failing.", ai->speed);
                        psnd_pcm_hw_params_free(setup);
                        return AL_FALSE; 
                }
        }


	/* Set number of periods. Periods used to be called fragments. */
	err = psnd_pcm_hw_params_set_periods_near(phandle, setup, &ai->periods, 0);
	if (err < 0) {
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_write_alsa: %s", psnd_strerror(err));
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

	err = psnd_pcm_hw_params_set_buffer_size_near(phandle, setup, &ai->bufframesize);
	if (err < 0) {
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_write_alsa: %s, size: %lu, speed: %d",
				psnd_strerror(err), ai->bufframesize, ai->speed);
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

#if (SND_LIB_MAJOR == 0)
	buffer_size = psnd_pcm_hw_params_get_buffer_size(setup);
	period_size = psnd_pcm_hw_params_get_period_size(setup, &dir);
#else
	psnd_pcm_hw_params_get_buffer_size(setup, &buffer_size);
	psnd_pcm_hw_params_get_period_size(setup, &period_size, &dir);
#endif

	_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
			"set_write_alsa (info): Buffersize = %lu (%u)",buffer_size, *bufsiz);
	_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
			"set_write_alsa (info): Periodsize = %lu", period_size);

	err = psnd_pcm_hw_params(phandle, setup);
	if(err < 0)
	{
		_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
				"set_alsa: %s", psnd_strerror(err));
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

	err = psnd_pcm_prepare(phandle);
	if(err < 0)
	{
		_alDebug(ALD_MAXIMUS,  __FILE__, __LINE__,
				"set_alsa %s", psnd_strerror(err));
		psnd_pcm_hw_params_free(setup);
		return AL_FALSE;
	}

	_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
		 "set_write_alsa: handle: %p, phandle: %p", handle, (void*)phandle);
	ai->setup_write = 1;

	psnd_pcm_hw_params_free(setup);
	return AL_TRUE;
}

static ALboolean
alcBackendSetAttributesALSA_( void *handle, ALuint *bufsiz, ALenum *fmt, ALuint *speed)
{
	return ((struct alsa_info *)handle)->mode == ALC_OPEN_INPUT_ ?
		set_read_alsa(handle, bufsiz, fmt, speed) :
		set_write_alsa(handle, bufsiz, fmt, speed);
}

static void alsa_blitbuffer(void *handle, const void *data, int bytes)
{
	struct alsa_info *ai = handle;
	snd_pcm_t *phandle = 0;
	const char *pdata = data;
	int data_len = bytes;
	int channels = 0;
	int err;
	snd_pcm_uframes_t frames;

	if((ai == NULL) || (ai->handle == NULL) || (!ai->setup_write))
	{
		return;
	}

	phandle = ai->handle;
	channels= ai->channels;
	frames  = (snd_pcm_uframes_t) bytes / ai->framesize;

	while(data_len > 0)
	{
		err = psnd_pcm_writei(phandle, pdata, frames);
		switch(err)
		{
			case -EAGAIN:
				continue;
				break;
			case -ESTRPIPE:
				do
				{
					err = psnd_pcm_resume(phandle);
				} while ( err == -EAGAIN );
				break;
                        case -EPIPE:
                                break;
			default:
                                if (err<0) {
                                        fprintf(stderr,"alsa_blitbuffer: Could not write audio data to sound device: %s\n",
                                        	psnd_strerror(err));
                                        break;
                                }
                                pdata += err * ai->framesize;
				data_len -= err * ai->framesize;
                                frames -= err;
				break;
		}
		if(err < 0)
		{
                        err = psnd_pcm_prepare(phandle);
			if(err < 0)
			{
				const char *serr = psnd_strerror(err);

				_alDebug(ALD_MAXIMUS, __FILE__, __LINE__,
                                 "alsa_blitbuffer: %s", serr);

				return;
			}
		}
	}

	return;
}

/* capture data from the audio device */
static ALsizei capture_alsa(void *handle,
		void *capture_buffer,
		int bufsize)
{
	struct alsa_info *ai = handle;
	snd_pcm_t *phandle = 0;
	char *pdata = capture_buffer;
	int ret;
	snd_pcm_uframes_t frames;

	if ((ai == NULL) || (ai->handle == NULL) || (!ai->setup_read))
		return 0;

	phandle = ai->handle;
	frames = (snd_pcm_uframes_t) bufsize / ai->framesize;

grab:
	ret = psnd_pcm_readi (phandle, pdata, frames);
	if (ret < 0) {
		if (ret == -EAGAIN)
			return 0;
		else if (ret == -EPIPE) {
			fprintf(stderr, "Error, overrun occurred, trying to recover.\n");
			ret = psnd_pcm_prepare(phandle);
			if (ret < 0)
				fprintf(stderr, "Unable to recover: %d\n", ret);
			else
				goto grab;
		} else {
			fprintf(stderr, "Unknown error occurred: %d.\n", ret);
		}
		return 0;
	} else
		return ret * ai->framesize;
}


static int AL2ALSAFMT(ALenum format) {
	switch(format) {
		case AL_FORMAT_STEREO8:     return SND_PCM_FORMAT_U8;
		case AL_FORMAT_MONO8:       return SND_PCM_FORMAT_U8;
		case AL_FORMAT_QUAD8_LOKI:  return SND_PCM_FORMAT_U8;
		case AL_FORMAT_STEREO16:    return SND_PCM_FORMAT_S16;
		case AL_FORMAT_MONO16:      return SND_PCM_FORMAT_S16;
		case AL_FORMAT_QUAD16_LOKI: return SND_PCM_FORMAT_S16;

		default: break;
	}
	return -1;
}



static int FRAMESIZE(snd_pcm_format_t fmt, unsigned int chans) {
	int retval = 0;
	switch(fmt) {
		case SND_PCM_FORMAT_U8:
			retval = 1;
			break;
		case SND_PCM_FORMAT_S16:
			retval = 2;
			break;
		default:
	                return -1;
		        break;
		}

	return(retval*chans);
}

static void
pause_alsa( UNUSED(void *handle) )
{
}

static void
resume_alsa( UNUSED(void *handle) )
{
}

static ALfloat
get_alsachannel( UNUSED(void *handle), UNUSED(ALuint channel) )
{
	return 0.0;
}

static int
set_alsachannel( UNUSED(void *handle), UNUSED(ALuint channel), UNUSED(ALfloat volume) )
{
	return 0;
}

static ALC_BackendOps alsaOps = {
	release_alsa,
	pause_alsa,
	resume_alsa,
	alcBackendSetAttributesALSA_,
	alsa_blitbuffer,
	capture_alsa,
	get_alsachannel,
	set_alsachannel
};

void
alcBackendOpenALSA_ (ALC_OpenMode mode, ALC_BackendOps **ops,
		     ALC_BackendPrivateData **privateData)
{
	*privateData = (mode == ALC_OPEN_INPUT_) ? grab_read_alsa() : grab_write_alsa();
	if (*privateData != NULL) {
		*ops = &alsaOps;
	}
}

#endif /* USE_BACKEND_ALSA */
