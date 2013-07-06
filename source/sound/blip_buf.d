/* blip_buf $vers. http://www.slack.net/~ant/                         */

/*  Modified for Genesis Plus GX by EkeEke (01/09/12)                 */
/*    - disabled assertions checks (define #BLIP_ASSERT to re-enable) */
/*    - fixed multiple time-frames support & removed m->avail         */
/*    - modified blip_read_samples to always output to stereo streams */
/*    - added blip_mix_samples function (see blip_buf.h)              */

import blip_buf;
import types;


/* Library Copyright (C) 2003-2009 Shay Green. This library is free software;
you can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
library is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */


/** Maximum clock_rate/sample_rate ratio. For a given sample_rate,
clock_rate must not be greater than sample_rate*blip_max_ratio. */
const int blip_max_ratio = 1 << 20;

/** Maximum number of samples that can be generated from one time frame. */
const int blip_max_frame = 4000;

version(BLARGG_TEST) && version(BLARGG_TEST) {
	import blargg_test.d;
}

alias u64 fixed_t;
const int pre_shift = 32;
//alias u32 fixed_t;
//const int pre_shift = 0;

const int time_bits = pre_shift + 20;

static fixed_t const time_unit = (fixed_t) 1 << time_bits;

const int bass_shift  = 9; /* affects high-pass filter breakpoint frequency */
const int end_frame_extra = 2; /* allows deltas slightly after frame length */

const int half_width  = 8;
const int buf_extra   = half_width*2 + end_frame_extra;
const int phase_bits  = 5;
const int phase_count = 1 << phase_bits;
const int delta_bits  = 15;
const int delta_unit  = 1 << delta_bits;
const int frac_bits = time_bits - pre_shift;

/* We could eliminate avail and encode whole samples in offset, but that would
limit the total buffered samples to blip_max_frame. That could only be
increased by decreasing time_bits, which would reduce resample ratio accuracy.
*/

/** First parameter of most functions is blip_t*, or const blip_t* if nothing
is changed. */
struct blip_t
{
	fixed_t factor;
	fixed_t offset;
	int size;
	int integrator;
}

alias int buf_t;

/* probably not totally portable */
buf_t* SAMPLES(blip_t* buf) {
    return (buf_t*) (buf + 1);
}

/* Arithmetic (sign-preserving) right shift */
int ARITH_SHIFT(int n, int shift) {
	return n >> shift;
}

const int max_sample = +32767;
const int min_sample = -32768;

int CLAMP(int n) {
	if (n > max_sample ) {
		return max_sample;
	} else if (n < min_sample) {
		return min_sample;
	} else {
		return n;
	}
}

version(BLIP_ASSERT) {
static void check_assumptions( void )
{
	int n;
	
	#if INT_MAX < 0x7FFFFFFF || UINT_MAX < 0xFFFFFFFF
		#error "int must be at least 32 bits"
	#endif
	
	assert( (-3 >> 1) == -2 ); /* right shift must preserve sign */
	
	n = max_sample * 2;
	n = CLAMP( n );
	assert( n == max_sample );
	
	n = min_sample * 2;
	n = CLAMP( n );
	assert( n == min_sample );
	
	assert( blip_max_ratio <= time_unit );
	assert( blip_max_frame <= (fixed_t) -1 >> time_bits );
}
}

/** Creates new buffer that can hold at most sample_count samples. Sets rates
so that there are blip_max_ratio clocks per sample. Returns pointer to new
buffer, or NULL if insufficient memory. */
blip_t* blip_new( int size )
{
	blip_t* m;
version(BLIP_ASSERT) {
	assert( size >= 0 );
}

	m = (blip_t*) malloc( sizeof *m + (size + buf_extra) * sizeof (buf_t) );
	if ( m )
	{
		m.factor = time_unit / blip_max_ratio;
		m.size   = size;
		blip_clear( m );
version(BLIP_ASSERT) {
		check_assumptions();
}
  }
	return m;
}

/** Frees buffer. No effect if NULL is passed. */
void blip_delete( blip_t* m )
{
	if ( m != null )
	{
		/* Clear fields in case user tries to use after freeing */
		memset( m, 0, sizeof *m );
		free( m );
	}
}

/** Sets approximate input clock rate and output sample rate. For every
clock_rate input clocks, approximately sample_rate samples are generated. */
void blip_set_rates( blip_t* m, double clock_rate, double sample_rate )
{
	double factor = time_unit * sample_rate / clock_rate;
	m.factor = (fixed_t) factor;
	
version(BLIP_ASSERT) {
	/* Fails if clock_rate exceeds maximum, relative to sample_rate */
	assert( 0 <= factor - m.factor && factor - m.factor < 1 );
}
  
/* Avoid requiring math.h. Equivalent to
	m.factor = (int) ceil( factor ) */
	if ( m.factor < factor )
		m.factor++;
	
	/* At this point, factor is most likely rounded up, but could still
	have been rounded down in the floating-point calculation. */
}

/** Clears entire buffer. Afterwards, blip_samples_avail() == 0. */
void blip_clear( blip_t* m )
{
	/* We could set offset to 0, factor/2, or factor-1. 0 is suitable if
	factor is rounded up. factor-1 is suitable if factor is rounded down.
	Since we don't know rounding direction, factor/2 accommodates either,
	with the slight loss of showing an error in half the time. Since for
	a 64-bit factor this is years, the halving isn't a problem. */
	
	m.offset     = m.factor / 2;
	m.integrator = 0;
	memset( SAMPLES( m ), 0, (m.size + buf_extra) * sizeof (buf_t) );
}

/** Length of time frame, in clocks, needed to make sample_count additional
samples available. */
int blip_clocks_needed( const blip_t* m, int samples )
{
	fixed_t needed;
	
version(BLIP_ASSERT) {
	/* Fails if buffer can't hold that many more samples */
	assert( (samples >= 0) && (((m.offset >> time_bits) + samples) <= m.size) );
}

  needed = (fixed_t) samples * time_unit;
	if ( needed < m.offset )
		return 0;
	
	return (needed - m.offset + m.factor - 1) / m.factor;
}

/** Makes input clocks before clock_duration available for reading as output
samples. Also begins new time frame at clock_duration, so that clock time 0 in
the new time frame specifies the same clock as clock_duration in the old time
frame specified. Deltas can have been added slightly past clock_duration (up to
however many clocks there are in two output samples). */
void blip_end_frame( blip_t* m, unsigned t )
{
	m.offset += t * m.factor;
	
version(BLIP_ASSERT) {
	/* Fails if buffer size was exceeded */
  assert( (m.offset >> time_bits) <= m.size );
}
}

/** Number of buffered samples available for reading. */
int blip_samples_avail( const blip_t* m )
{
	return (m.offset >> time_bits);
}

static void remove_samples( blip_t* m, int count )
{
	buf_t* buf = SAMPLES( m );
	int remain = (m.offset >> time_bits) + buf_extra - count;
  m.offset -= count * time_unit;
  
	memmove( &buf [0], &buf [count], remain * sizeof buf [0] );
	memset( &buf [remain], 0, count * sizeof buf [0] );
}

/** Reads and removes at most 'count' samples and writes them to to every other 
element of 'out', allowing easy interleaving of two buffers into a stereo sample
stream. Outputs 16-bit signed samples. Returns number of samples actually read.  */
int blip_read_samples( blip_t* m, s16[] out_var, int count)
{
version(BLIP_ASSERT) {
	assert( count >= 0 );
	
	if ( count > (m.offset >> time_bits) )
		count = m.offset >> time_bits;
	
	if ( count )
}
  {
		const buf_t* in_var  = SAMPLES( m );
		const buf_t* end = in_var + count;
		int sum = m.integrator;
		do
		{
			/* Eliminate fraction */
			int s = ARITH_SHIFT( sum, delta_bits );
			
			sum += *in_var++;
			
			s = CLAMP( s );
			
			*out_var = s;
			out_var += 2;
			
			/* High-pass filter */
			sum -= s << (delta_bits - bass_shift);
		}
		while ( in_var != end );
		m.integrator = sum;
		
		remove_samples( m, count );
	}
	
	return count;
}

/* Same as above function except sample is added to output buffer previous value */
/* This allows easy mixing of different blip buffers into a single output stream */
int blip_mix_samples( blip_t* m, s16[] out_var, int count)
{
version(BLIP_ASSERT) {
	assert( count >= 0 );
	
	if ( count > (m.offset >> time_bits) )
		count = m.offset >> time_bits;
	
	if ( count )
}
  {
		const buf_t* in_var  = SAMPLES( m );
		const buf_t* end = in_var + count;
		int sum = m.integrator;
		do
		{
			/* Eliminate fraction */
			int s = ARITH_SHIFT( sum, delta_bits );
			
			sum += *in_var++;
			
			/* High-pass filter */
			sum -= s << (delta_bits - bass_shift);

            /* Add current buffer value */
            s += *out_var;
			
			s = CLAMP( s );
			
			*out_var = s;
			out_var += 2;
		}
		while ( in_var != end );
		m.integrator = sum;
		
		remove_samples( m, count );
	}
	
	return count;
}

/* Things that didn't help performance on x86:
	__attribute__((aligned(128)))
	#define short int
	restrict
*/

/* Sinc_Generator( 0.9, 0.55, 4.5 ) */
static const s16[phase_count + 1][half_width] bl_step = [
[   43, -115,  350, -488, 1136, -914, 5861,21022],
[   44, -118,  348, -473, 1076, -799, 5274,21001],
[   45, -121,  344, -454, 1011, -677, 4706,20936],
[   46, -122,  336, -431,  942, -549, 4156,20829],
[   47, -123,  327, -404,  868, -418, 3629,20679],
[   47, -122,  316, -375,  792, -285, 3124,20488],
[   47, -120,  303, -344,  714, -151, 2644,20256],
[   46, -117,  289, -310,  634,  -17, 2188,19985],
[   46, -114,  273, -275,  553,  117, 1758,19675],
[   44, -108,  255, -237,  471,  247, 1356,19327],
[   43, -103,  237, -199,  390,  373,  981,18944],
[   42,  -98,  218, -160,  310,  495,  633,18527],
[   40,  -91,  198, -121,  231,  611,  314,18078],
[   38,  -84,  178,  -81,  153,  722,   22,17599],
[   36,  -76,  157,  -43,   80,  824, -241,17092],
[   34,  -68,  135,   -3,    8,  919, -476,16558],
[   32,  -61,  115,   34,  -60, 1006, -683,16001],
[   29,  -52,   94,   70, -123, 1083, -862,15422],
[   27,  -44,   73,  106, -184, 1152,-1015,14824],
[   25,  -36,   53,  139, -239, 1211,-1142,14210],
[   22,  -27,   34,  170, -290, 1261,-1244,13582],
[   20,  -20,   16,  199, -335, 1301,-1322,12942],
[   18,  -12,   -3,  226, -375, 1331,-1376,12293],
[   15,   -4,  -19,  250, -410, 1351,-1408,11638],
[   13,    3,  -35,  272, -439, 1361,-1419,10979],
[   11,    9,  -49,  292, -464, 1362,-1410,10319],
[    9,   16,  -63,  309, -483, 1354,-1383, 9660],
[    7,   22,  -75,  322, -496, 1337,-1339, 9005],
[    6,   26,  -85,  333, -504, 1312,-1280, 8355],
[    4,   31,  -94,  341, -507, 1278,-1205, 7713],
[    3,   35, -102,  347, -506, 1238,-1119, 7082],
[    1,   40, -110,  350, -499, 1190,-1021, 6464],
[    0,   43, -115,  350, -488, 1136, -914, 5861]
];

/* Shifting by pre_shift allows calculation using u32 rather than
possibly-wider fixed_t. On 32-bit platforms, this is likely more efficient.
And by having pre_shift 32, a 32-bit platform can easily do the shift by
simply ignoring the low half. */

/** Adds positive/negative delta into buffer at specified clock time. */
void blip_add_delta( blip_t* m, unsigned time, int delta )
{
	unsigned fixed = (unsigned) ((time * m.factor + m.offset) >> pre_shift);
	buf_t* out_var = SAMPLES( m ) + (fixed >> frac_bits);
	
	const int phase_shift = frac_bits - phase_bits;
	int phase = fixed >> phase_shift & (phase_count - 1);
	const s16* in_var  = bl_step [phase];
	const s16* rev = bl_step [phase_count - phase];
	
	int interp = fixed >> (phase_shift - delta_bits) & (delta_unit - 1);
	int delta2 = (delta * interp) >> delta_bits;
	delta -= delta2;
	
version(BLIP_ASSERT) {
	/* Fails if buffer size was exceeded */
	assert( out_var <= &SAMPLES( m ) [m.size + end_frame_extra] );
}

	out_var [0] += in_var[0]*delta + in_var[half_width+0]*delta2;
	out_var [1] += in_var[1]*delta + in_var[half_width+1]*delta2;
	out_var [2] += in_var[2]*delta + in_var[half_width+2]*delta2;
	out_var [3] += in_var[3]*delta + in_var[half_width+3]*delta2;
	out_var [4] += in_var[4]*delta + in_var[half_width+4]*delta2;
	out_var [5] += in_var[5]*delta + in_var[half_width+5]*delta2;
	out_var [6] += in_var[6]*delta + in_var[half_width+6]*delta2;
	out_var [7] += in_var[7]*delta + in_var[half_width+7]*delta2;
	
	in_var = rev;
	out_var [ 8] += in_var[7]*delta + in_var[7-half_width]*delta2;
	out_var [ 9] += in_var[6]*delta + in_var[6-half_width]*delta2;
	out_var [10] += in_var[5]*delta + in_var[5-half_width]*delta2;
	out_var [11] += in_var[4]*delta + in_var[4-half_width]*delta2;
	out_var [12] += in_var[3]*delta + in_var[3-half_width]*delta2;
	out_var [13] += in_var[2]*delta + in_var[2-half_width]*delta2;
	out_var [14] += in_var[1]*delta + in_var[1-half_width]*delta2;
	out_var [15] += in_var[0]*delta + in_var[0-half_width]*delta2;
}

/** Same as blip_add_delta(), but uses faster, lower-quality synthesis. */
void blip_add_delta_fast( blip_t* m, unsigned time, int delta )
{
	unsigned fixed = (unsigned) ((time * m.factor + m.offset) >> pre_shift);
	buf_t* out_var = SAMPLES( m ) + (fixed >> frac_bits);
	
	int interp = fixed >> (frac_bits - delta_bits) & (delta_unit - 1);
	int delta2 = delta * interp;
	
version(BLIP_ASSERT) {
  /* Fails if buffer size was exceeded */
	assert( out_var <= &SAMPLES( m ) [m.size + end_frame_extra] );
}
  
	out_var [7] += delta * delta_unit - delta2;
	out_var [8] += delta2;
}
