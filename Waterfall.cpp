
#include <cstdio>
#include <cmath>
#include <complex>
#include <liquid.h>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "Waterfall.h"
#include "sdrberry.h"

using namespace std;

const int	noise_floor {20};
const int	hor_lines {8};
const int	vert_lines {9};

IQSample::value_type rms_level_approx(const IQSampleVector& samples)
{
	unsigned int n = samples.size();
	n = (n + 63) / 64;

	IQSample::value_type level = 0;
	for (unsigned int i = 0; i < n; i++) {
		const IQSample& s = samples[i];
		IQSample::value_type re = s.real(), im = s.imag();
		level += re * re + im * im;
	}

	return sqrt(level / n);
}


Fft_calculator	Fft_calc;
Waterfall		Wf;

Fft_calculator::Fft_calculator()
{
	
}

Fft_calculator::~Fft_calculator()
{

}

/*
 *	Output of fft is:
 *	x[0] = DC component
 *	x[1] to x[n/2] +ve frequencies
 *  x[n/2] to x[n] -ve frequencies
 *  
 *	x[n/2] Nyquist frequency
 *
 **/

void	Fft_calculator::process_samples(const IQSampleVector&	input)
{
	m_input.insert(m_input.end(), input.begin(), input.end());
	if (m_input.size() >= nfft)
	{
		std::unique_lock<std::mutex> lock(m_mutex); 
		// Apply hamming window
		for (int i = 0; i < nfft; i++)
		{
			m_input[i].real(m_input[i].real() * v_window[i]);
			m_input[i].imag(m_input[i].imag() * v_window[i]);
		}
		plan = fft_create_plan(nfft, m_input.data(), fft_output.data(), type, flags);
		fft_execute(plan);
		//std::rotate(fft_output.begin(), fft_output.begin() + nfft / 2, fft_output.end());
		fft_destroy_plan(plan);
		m_input.clear();
	}
}

void	Fft_calculator::plan_fft(int size)
{
	nfft = size;
	fft_output.reserve(nfft); 
	m_input.reserve(nfft);
	fft_output.resize(nfft); 
	v_window.clear();
	for (int i = 0; i < nfft; i++)
	{
		v_window.insert(v_window.end(), liquid_windowf(LIQUID_WINDOW_HAMMING, i, nfft, 0)) ;		
	}
}

static void draw_event_cb(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * obj = lv_event_get_target(e);
	
	if (code == LV_EVENT_DRAW_PART_BEGIN) 
	{
		lv_obj_draw_part_dsc_t * dsc = (lv_obj_draw_part_dsc_t *)lv_event_get_param(e);
		/*Set the markers' text*/
		if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_X) 
		{
			string	str[vert_lines];
			long long f = vfo.get_sdr_frequency();
				
			//int ii = (int)floor((ifrate / 2.0) / (float)hor_lines / (float)(nfft_samples / 2)); 
			int ii = (int)floor((ifrate / 2.0) / (float)(vert_lines -1)); 
				
			for (int i = 0; i < vert_lines; i++)
			{
				char	str1[20];
				double l = (double)((f / 10ULL) % 1000ULL) / 100.0;
				sprintf(str1, "%4.2f", l);
				str[i] = string(str1);
				size_t pos = str[i].length();
				f += ii;
			}
			lv_snprintf(dsc->text, sizeof(dsc->text), "%s", str[dsc->value].c_str());
		}
	}
}


void Waterfall::init(lv_obj_t* scr, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
	lv_style_init(&waterfall_style);
	lv_style_set_radius(&waterfall_style, 0);
	lv_style_set_bg_color(&waterfall_style, lv_color_black());
	
	chart = lv_chart_create(scr);
	lv_obj_add_style(chart, &waterfall_style, 0); 
	lv_obj_set_pos(chart, x, y); 
	lv_obj_set_size(chart, w, h - 100);
	lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
	lv_obj_set_style_pad_hor(scr, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_ver(scr, 0, LV_PART_MAIN);
	
	//lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 0, 6, 1, true, 80);
	lv_chart_set_div_line_count(chart, hor_lines, vert_lines); 
	lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 0, 0, vert_lines, 1, true, 50);
    lv_obj_add_event_cb(chart, draw_event_cb, LV_EVENT_ALL, NULL);
	m_cursor = lv_chart_add_cursor(chart, lv_palette_main(LV_PALETTE_BLUE),  LV_DIR_BOTTOM);
	
	lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
	ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

void Waterfall::set_pos(int32_t  offset)
{
	uint32_t pos;
		
	float div = ifrate / nfft_samples;
	pos = (uint32_t)round(offset / div);
	lv_chart_set_cursor_point(chart, m_cursor, NULL, pos);
}

void Waterfall::load_data()
{
	int i = data_set.size();
	if (i > 0)
	{
		lv_chart_set_point_count(chart, i);
		lv_chart_set_ext_y_array(chart, ser, (lv_coord_t *)data_set.data());	
		lv_chart_refresh(chart);
	}
}


void Fft_calculator::upload_fft(std::vector<lv_coord_t>& data_set)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	int i = 0, av = 0;
	data_set.resize(fft_output.size()/2);
	int start = fft_output.size() / 2;
		
	for (auto &col : fft_output)
	{
		if (i == (fft_output.size() / 2))
			break;
		//if (i > 0)
			data_set[i] = noise_floor + (lv_coord_t)10 * log10((col.real() * col.real() + col.imag() * col.imag()));
		i++;
	}
}

void Fft_calculator::set_signal_strength(double strength)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	signal_strength =  20* log10(strength);
	//printf(" signal_strength %f \n", signal_strength);
}

double Fft_calculator::get_signal_strength()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	return signal_strength ;
}
