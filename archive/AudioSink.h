#pragma once

#include <cstdio>	//stdandard output
#include <cstdlib>
#include <alsa/asoundlib.h>

class AudioSink
{
public:
	int			init(void);
				AudioSink();
				~AudioSink();
	int			xrun_recovery(snd_pcm_t *handle, int err);
	void		generate_sine(const snd_pcm_channel_area_t *areas, snd_pcm_uframes_t offset, int count, double *_phase);
	
	// period_size is the minimum number of frames necessary to fill the buffer to playback (played in one interupt)
	snd_pcm_sframes_t	period_size;
	
private:	
	snd_pcm_t			*handle;
	snd_pcm_uframes_t	frames;
	unsigned int		rate = 44100;
	snd_pcm_sframes_t	buffer_size;
	snd_output_t		*output = NULL;
	int					period_event = 0;
	snd_pcm_format_t	format = SND_PCM_FORMAT_S16;
	unsigned int		channels = 1;
	double				freq = 440;
	int					resample = 1; 
	unsigned int buffer_time = 500000;       /* ring buffer length in us */
	unsigned int period_time = 100000;       /* period time in us */
	char				device[10];			// = "plughw:0,0"; 
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	snd_pcm_channel_area_t *areas;
	signed short		*samples;
	
	
	int		set_hwparams(snd_pcm_t *handle,	snd_pcm_hw_params_t *params,snd_pcm_access_t access);
	int		set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams);
	int		write_loop(snd_pcm_t *handle, signed short *samples, snd_pcm_channel_area_t *areas);
	int		wait_for_poll(snd_pcm_t *handle, struct pollfd *ufds, unsigned int count);
	int		write_and_poll_loop(snd_pcm_t *handle, signed short *samples, snd_pcm_channel_area_t *areas);
	int		direct_write_loop(snd_pcm_t *handle, signed short *samples, snd_pcm_channel_area_t *areas);	
	int		async_direct_loop(snd_pcm_t *handle, signed short *samples ATTRIBUTE_UNUSED, snd_pcm_channel_area_t *areas ATTRIBUTE_UNUSED);
};

extern AudioSink	audio_player;

void	 async_direct_callback(snd_async_handler_t *ahandler);