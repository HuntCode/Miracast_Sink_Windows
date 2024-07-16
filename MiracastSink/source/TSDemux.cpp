/*
依次输入188字节的MPEGTS数据，解析为H264+AAC数据

by Tam.Xie

2020.07.27
*/

#include "pch.h"
#include <cassert>
#include <cstdlib>    // for malloc free size_t
#include <cstring>    // memset memcpy memmove
#include <climits>    // for INT_MAX
#include <cerrno>
#include <cstring>        // for memset
#include <inttypes.h>
#include <cstddef>    // for size_t
#include <inttypes.h>
#include <stddef.h>
#include <map>
#include <vector>
#include <fcntl.h>
#include "TSDemux.h"

#define POSMAP_PTS_INTERVAL     270000LL
#define ES_INIT_BUFFER_SIZE     64000
#define ES_MAX_BUFFER_SIZE      1048576
#define PTS_MASK                0x1ffffffffLL
#define PTS_UNSET               0x1ffffffffLL
#define PTS_TIME_BASE           90000LL
#define RESCALE_TIME_BASE       1000000LL
#define FLUTS_NORMAL_TS_PACKETSIZE  188
#define FLUTS_M2TS_TS_PACKETSIZE    192
#define FLUTS_DVB_ASI_TS_PACKETSIZE 204
#define FLUTS_ATSC_TS_PACKETSIZE    208
#define AV_CONTEXT_PACKETSIZE       208
#define TS_CHECK_MIN_SCORE          2
#define TS_CHECK_MAX_SCORE          10
#define MAX_RESYNC_SIZE         65536
// PSI section size (EN 300 468)
#define TABLE_BUFFER_SIZE       4096

#define WXMEDIA_API

namespace TSDemux
{
	static int aac_sample_rates[16] =
	{
		96000, 88200, 64000, 48000, 44100, 32000,
		24000, 22050, 16000, 12000, 11025, 8000, 7350,
		0, 0, 0
	};

	static const int h264_lev2cpbsize[][2] =
	{
		{ 10, 175 },
		{ 11, 500 },
		{ 12, 1000 },
		{ 13, 2000 },
		{ 20, 2000 },
		{ 21, 4000 },
		{ 22, 4000 },
		{ 30, 10000 },
		{ 31, 14000 },
		{ 32, 20000 },
		{ 40, 25000 },
		{ 41, 62500 },
		{ 42, 62500 },
		{ 50, 135000 },
		{ 51, 240000 },
		{ -1, -1 },
	};
	class CBitstream
	{
	private:
		uint8_t       *m_data = nullptr;
		size_t         m_offset = 0;
		size_t   m_len = 0;
		bool           m_error = false;
		const bool     m_doEP3 = false;

	public:
		CBitstream(uint8_t *data, size_t bits)
		{
			m_data = data;
			m_len = bits;
		}

		// this is a bitstream that has embedded emulation_prevention_three_byte
		// sequences that need to be removed as used in HECV.
		// Data must start at byte 2
		CBitstream(uint8_t *data, size_t bits, bool doEP3)
			: m_data(data)
			, m_offset(16) // skip header and use as sentinel for EP3 detection
			, m_len(bits)
			, m_error(false)
			, m_doEP3(true)
		{}

		void skipBits(unsigned int num)
		{
			if (m_doEP3)
			{

				while (num)
				{
					unsigned int tmp = (unsigned int)(m_offset >> 3);
					if (!(m_offset & 7) && (m_data[tmp--] == 3) && (m_data[tmp--] == 0) && (m_data[tmp] == 0))
						m_offset += 8;   // skip EP3 byte

					if (!(m_offset & 7) && (num >= 8)) // byte boundary, speed up things a little bit
					{
						m_offset += 8;
						num -= 8;
					}
					else if ((tmp = 8 - (m_offset & 7)) <= num) // jump to byte boundary
					{
						m_offset += tmp;
						num -= tmp;
					}
					else
					{
						m_offset += num;
						num = 0;
					}

					if (m_offset >= m_len)
					{
						m_error = true;
						break;
					}
				}

				return;
			}

			m_offset += num;
		}

		unsigned int readBits(int num)
		{
			unsigned int r = 0;

			while (num > 0)
			{
				if (m_doEP3)
				{
					size_t tmp = m_offset >> 3;
					if (!(m_offset & 7) && (m_data[tmp--] == 3) && (m_data[tmp--] == 0) && (m_data[tmp] == 0))
						m_offset += 8;   // skip EP3 byte
				}

				if (m_offset >= m_len)
				{
					m_error = true;
					return 0;
				}

				num--;

				if (m_data[m_offset / 8] & (1 << (7 - (m_offset & 7))))
					r |= 1 << num;

				m_offset++;
			}
			return r;
		}

		unsigned int showBits(int num)
		{
			unsigned int r = 0;
			size_t offs = m_offset;

			while (num > 0)
			{
				if (offs >= m_len)
				{
					m_error = true;
					return 0;
				}

				num--;

				if (m_data[offs / 8] & (1 << (7 - (offs & 7))))
					r |= 1 << num;

				offs++;
			}
			return r;
		}

		unsigned int readGolombUE(int maxbits = 32)
		{
			int lzb = -1;
			int bits = 0;

			for (int b = 0; !b; lzb++, bits++)
			{
				if (bits > maxbits)
					return 0;
				b = readBits1();
			}

			return (1 << lzb) - 1 + readBits(lzb);
		}

		signed int readGolombSE()
		{
			int v, pos;
			v = readGolombUE();
			if (v == 0)
				return 0;

			pos = (v & 1);
			v = (v + 1) >> 1;
			return pos ? v : -v;
		}
		unsigned int readBits1() { return readBits(1); }
		size_t       length() { return m_len; }
		bool         isError() { return m_error; }
	};

	enum STREAM_TYPE
	{
		STREAM_TYPE_UNKNOWN = 0,
		STREAM_TYPE_VIDEO_MPEG1,
		STREAM_TYPE_VIDEO_MPEG2,
		STREAM_TYPE_AUDIO_MPEG1,
		STREAM_TYPE_AUDIO_MPEG2,
		STREAM_TYPE_AUDIO_AAC,
		STREAM_TYPE_AUDIO_AAC_ADTS,
		STREAM_TYPE_AUDIO_AAC_LATM,
		STREAM_TYPE_VIDEO_H264,
		STREAM_TYPE_VIDEO_HEVC,
		STREAM_TYPE_AUDIO_AC3,
		STREAM_TYPE_AUDIO_EAC3,
		STREAM_TYPE_DVB_TELETEXT,
		STREAM_TYPE_DVB_SUBTITLE,
		STREAM_TYPE_VIDEO_MPEG4,
		STREAM_TYPE_VIDEO_VC1,
		STREAM_TYPE_AUDIO_LPCM,
		STREAM_TYPE_AUDIO_DTS,
		STREAM_TYPE_PRIVATE_DATA
	};

	struct STREAM_INFO
	{
		char                  language[4];
		int                   composition_id;
		int                   ancillary_id;
		int                   fps_scale;
		int                   fps_rate;
		int                   height;
		int                   width;
		float                 aspect;
		int                   channels;
		int                   sample_rate;
		int                   block_align;
		int                   bit_rate;
		int                   bits_per_sample;
		bool                  interlaced;
	};

	struct STREAM_PKT
	{
		uint16_t              pid;
		size_t                size;
		const uint8_t*  data;
		uint64_t              dts;
		uint64_t              pts;
		uint64_t              duration;
		bool                  streamChange;
	};

	class ElementaryStream
	{
	public:
		ElementaryStream(uint16_t pes_pid);
		virtual ~ElementaryStream();
		virtual void Reset();
		void ClearBuffer();
		int Append(const uint8_t* buf, size_t len, bool new_pts = false);
		const char* GetStreamCodecName() const;
		static const char* GetStreamCodecName(STREAM_TYPE stream_type);

		uint16_t pid;
		STREAM_TYPE stream_type;
		uint64_t c_dts;               ///< current MPEG stream DTS (decode time for video)
		uint64_t c_pts;               ///< current MPEG stream PTS (presentation time for audio and video)
		uint64_t p_dts;               ///< previous MPEG stream DTS (decode time for video)
		uint64_t p_pts;               ///< previous MPEG stream PTS (presentation time for audio and video)

		bool has_stream_info;         ///< true if stream info is completed else it requires parsing of iframe

		STREAM_INFO stream_info;

		bool GetStreamPacket(STREAM_PKT* pkt);
		virtual void Parse(STREAM_PKT* pkt);

	protected:
		void ResetStreamPacket(STREAM_PKT* pkt);
		uint64_t Rescale(uint64_t a, uint64_t b, uint64_t c);
		bool SetVideoInformation(int FpsScale, int FpsRate, int Height, int Width, float Aspect, bool Interlaced);
		bool SetAudioInformation(int Channels, int SampleRate, int BitRate, int BitsPerSample, int BlockAlign);

		size_t es_alloc_init;         ///< Initial allocation of memory for buffer
		uint8_t* es_buf;        ///< The Pointer to buffer
		size_t es_alloc;              ///< Allocated size of memory for buffer
		size_t es_len;                ///< Size of data in buffer
		size_t es_consumed;           ///< Consumed payload. Will be erased on next append
		size_t es_pts_pointer;        ///< Position in buffer where current PTS becomes applicable
		size_t es_parsed;             ///< Parser: Last processed position in buffer
		bool   es_found_frame;        ///< Parser: Found frame
		bool   es_frame_valid;
	};


	class TSTable
	{
	public:
		uint8_t table_id = 0xff;
		uint8_t version = 0xff;
		uint16_t id = 0xffff;
		uint16_t len = 0;
		uint16_t offset = 0;
		uint8_t buf[TABLE_BUFFER_SIZE];

		TSTable(void)
		{
			memset(buf, 0, TABLE_BUFFER_SIZE);
		}

		void Reset(void)
		{
			len = 0;
			offset = 0;
		}
	};

	enum PACKET_TYPE
	{
		PACKET_TYPE_UNKNOWN = 0,
		PACKET_TYPE_PSI,
		PACKET_TYPE_PES
	};

	class Packet
	{
	public:
		Packet(void)
			: pid(0xffff)
			, continuity(0xff)
			, packet_type(PACKET_TYPE_UNKNOWN)
			, channel(0)
			, wait_unit_start(true)
			, has_stream_data(false)
			, streaming(false)
			, stream(NULL)
			, packet_table()
		{
		}

		~Packet(void)
		{
			if (stream)
				delete stream;
		}

		void Reset(void)
		{
			continuity = 0xff;
			wait_unit_start = true;
			packet_table.Reset();
			if (stream)
				stream->Reset();
		}

		uint16_t pid;
		uint8_t continuity;
		PACKET_TYPE packet_type;
		uint16_t channel;
		bool wait_unit_start;
		bool has_stream_data;
		bool streaming;
		ElementaryStream* stream;
		TSTable packet_table;
	};


	enum {
		AVCONTEXT_TS_ERROR = -3,
		AVCONTEXT_IO_ERROR = -2,
		AVCONTEXT_TS_NOSYNC = -1,
		AVCONTEXT_CONTINUE = 0,
		AVCONTEXT_PROGRAM_CHANGE = 1,
		AVCONTEXT_STREAM_PID_DATA = 2,
		AVCONTEXT_DISCONTINUITY = 3
	};

	class AVContext
	{


	public:
		int m_pidAAC = 0;
		int m_pidH264 = 0;
	public:
		AVContext(uint64_t pos, uint16_t channel);
		void Reset(void);

		uint16_t GetPID() const;
		PACKET_TYPE GetPIDType() const;
		uint16_t GetPIDChannel() const;
		bool HasPIDStreamData() const;
		bool HasPIDPayload() const;
		ElementaryStream* GetPIDStream();
		std::vector<ElementaryStream*> GetStreams();
		void StartStreaming(uint16_t pid);
		void StopStreaming(uint16_t pid);

		ElementaryStream* GetStream(uint16_t pid) const;
		uint16_t GetChannel(uint16_t pid) const;
		void ResetPackets();

		// TS parser
		int TSResync(uint8_t* buf, int buf_size);
		uint64_t GoNext();
		uint64_t Shift();

		int ProcessTSPacket();
		int ProcessTSPayload();

	private:
		AVContext(const AVContext&);
		AVContext& operator=(const AVContext&);

		static STREAM_TYPE get_stream_type(uint8_t pes_type);
		static uint8_t     av_rb8(const uint8_t* p);
		static uint16_t    av_rb16(const uint8_t* p);
		static uint32_t    av_rb32(const uint8_t* p);
		static uint64_t    decode_pts(const uint8_t* p);
		void clear_pmt();
		void clear_pes(uint16_t channel);
		int parse_ts_psi();
		static STREAM_INFO parse_pes_descriptor(const uint8_t* p, size_t len, STREAM_TYPE* st);
		int parse_ts_pes();

		// Raw packet buffer
		uint64_t av_pos;
		size_t av_data_len;
		size_t av_pkt_size = 188;
		uint8_t av_buf[AV_CONTEXT_PACKETSIZE];

		// TS Streams context
		uint16_t m_channel;
		std::map<uint16_t, Packet> packets;

		// Packet context
		uint16_t pid;
		bool transport_error;
		bool has_payload;
		bool payload_unit_start;
		bool discontinuity;
		const uint8_t* payload;
		size_t payload_len;
		Packet* packet;
	};

	class ES_AAC : public ElementaryStream
	{
	private:
		int         m_SampleRate;
		int         m_Channels;
		int         m_BitRate;
		int         m_FrameSize;

		int64_t     m_PTS;                /* pts of the current frame */
		int64_t     m_DTS;                /* dts of the current frame */

		bool        m_Configured;
		int         m_AudioMuxVersion_A;
		int         m_FrameLengthType;

	public:

		uint32_t LATMGetValue(CBitstream *bs) { return bs->readBits(bs->readBits(2) * 8); }

		ES_AAC(uint16_t pes_pid)
			: ElementaryStream(pes_pid)
		{
			m_Configured = false;
			m_FrameLengthType = 0;
			m_PTS = 0;
			m_DTS = 0;
			m_FrameSize = 0;
			m_SampleRate = 0;
			m_Channels = 0;
			m_BitRate = 0;
			m_AudioMuxVersion_A = 0;
			es_alloc_init = 1920 * 2;
			Reset();
		}

		virtual ~ES_AAC()
		{
		}

		void Parse(STREAM_PKT* pkt)
		{
			size_t p = (int)es_parsed;
			size_t l;
			while ((l = es_len - p) > 8)
			{
				if (FindHeaders(es_buf + p, l) < 0)
					break;
				p++;
			}
			es_parsed = p;

			if (es_found_frame && l >= m_FrameSize)
			{
				bool streamChange = SetAudioInformation(m_Channels, m_SampleRate, m_BitRate, 0, 0);
				pkt->pid = pid;
				pkt->data = &es_buf[p];
				pkt->size = m_FrameSize;
				pkt->duration = 1024 * 90000 / (!m_SampleRate ? aac_sample_rates[4] : m_SampleRate);
				pkt->dts = m_DTS;
				pkt->pts = m_PTS;
				pkt->streamChange = streamChange;

				es_consumed = p + m_FrameSize;
				es_parsed = es_consumed;
				es_found_frame = false;
			}
		}

		int  FindHeaders(uint8_t *buf, size_t buf_size)
		{
			if (es_found_frame)
				return -1;

			uint8_t *buf_ptr = buf;

			if (stream_type == STREAM_TYPE_AUDIO_AAC)
			{
				if (buf_ptr[0] == 0xFF && (buf_ptr[1] & 0xF0) == 0xF0)
					stream_type = STREAM_TYPE_AUDIO_AAC_ADTS;
				else if (buf_ptr[0] == 0x56 && (buf_ptr[1] & 0xE0) == 0xE0)
					stream_type = STREAM_TYPE_AUDIO_AAC_LATM;
			}

			// STREAM_TYPE_AUDIO_AAC_LATM
			if (stream_type == STREAM_TYPE_AUDIO_AAC_LATM)
			{
				if ((buf_ptr[0] == 0x56 && (buf_ptr[1] & 0xE0) == 0xE0))
				{
					// TODO
					if (buf_size < 16)
						return -1;

					CBitstream bs(buf_ptr, 16 * 8);
					bs.skipBits(11);
					m_FrameSize = bs.readBits(13) + 3;
					if (!ParseLATMAudioMuxElement(&bs))
						return 0;

					es_found_frame = true;
					m_DTS = c_pts;
					m_PTS = c_pts;
					c_pts += 90000 * 1024 / (!m_SampleRate ? aac_sample_rates[4] : m_SampleRate);
					return -1;
				}
			}
			// STREAM_TYPE_AUDIO_AAC_ADTS
			else if (stream_type == STREAM_TYPE_AUDIO_AAC_ADTS)
			{
				if (buf_ptr[0] == 0xFF && (buf_ptr[1] & 0xF0) == 0xF0)
				{
					// need at least 7 bytes for header
					if (buf_size < 7)
						return -1;

					CBitstream bs(buf_ptr, 9 * 8);
					bs.skipBits(15);

					// check if CRC is present, means header is 9 byte long
					int noCrc = bs.readBits(1);
					if (!noCrc && (buf_size < 9))
						return -1;

					bs.skipBits(2); // profile
					int SampleRateIndex = bs.readBits(4);
					bs.skipBits(1); // private
					m_Channels = bs.readBits(3);
					bs.skipBits(4);

					m_FrameSize = bs.readBits(13);
					m_SampleRate = aac_sample_rates[SampleRateIndex & 0x0F];

					es_found_frame = true;
					m_DTS = c_pts;
					m_PTS = c_pts;
					c_pts += 90000 * 1024 / (!m_SampleRate ? aac_sample_rates[4] : m_SampleRate);
					return -1;
				}
			}
			return 0;
		}

		bool ParseLATMAudioMuxElement(CBitstream *bs)
		{
			if (!bs->readBits1())
				ReadStreamMuxConfig(bs);

			if (!m_Configured)
				return false;

			return true;
		}

		void ReadStreamMuxConfig(CBitstream *bs)
		{
			int AudioMuxVersion = bs->readBits(1);
			m_AudioMuxVersion_A = 0;
			if (AudioMuxVersion)                       // audioMuxVersion
				m_AudioMuxVersion_A = bs->readBits(1);

			if (m_AudioMuxVersion_A)
				return;

			if (AudioMuxVersion)
				LATMGetValue(bs);                      // taraFullness

			bs->skipBits(1);                         // allStreamSameTimeFraming = 1
			bs->skipBits(6);                         // numSubFrames = 0
			bs->skipBits(4);                         // numPrograms = 0

													 // for each program (which there is only on in DVB)
			bs->skipBits(3);                         // numLayer = 0

													 // for each layer (which there is only on in DVB)
			if (!AudioMuxVersion)
				ReadAudioSpecificConfig(bs);
			else
				return;

			// these are not needed... perhaps
			m_FrameLengthType = bs->readBits(3);
			switch (m_FrameLengthType)
			{
			case 0:
				bs->readBits(8);
				break;
			case 1:
				bs->readBits(9);
				break;
			case 3:
			case 4:
			case 5:
				bs->readBits(6);                 // celp_table_index
				break;
			case 6:
			case 7:
				bs->readBits(1);                 // hvxc_table_index
				break;
			}

			if (bs->readBits(1))
			{                   // other data?
				int esc;
				do
				{
					esc = bs->readBits(1);
					bs->skipBits(8);
				} while (esc);
			}

			if (bs->readBits(1))                   // crc present?
				bs->skipBits(8);                     // config_crc
			m_Configured = true;
		}

		void ReadAudioSpecificConfig(CBitstream *bs)
		{
			int aot = bs->readBits(5);
			if (aot == 31)
				aot = 32 + bs->readBits(6);

			int SampleRateIndex = bs->readBits(4);

			if (SampleRateIndex == 0xf)
				m_SampleRate = bs->readBits(24);
			else
				m_SampleRate = aac_sample_rates[SampleRateIndex & 0xf];

			m_Channels = bs->readBits(4);

			if (aot == 5) { // AOT_SBR
				if (bs->readBits(4) == 0xf) { // extensionSamplingFrequencyIndex
					bs->skipBits(24);
				}
				aot = bs->readBits(5); // this is the main object type (i.e. non-extended)
				if (aot == 31)
					aot = 32 + bs->readBits(6);
			}

			if (aot != 2)
				return;

			bs->skipBits(1);      //framelen_flag
			if (bs->readBits1())  // depends_on_coder
				bs->skipBits(14);

			if (bs->readBits(1))  // ext_flag
				bs->skipBits(1);    // ext3_flag
		}

		void Reset()
		{
			ElementaryStream::Reset();
			m_Configured = false;
		}

	};

	class ES_h264 : public ElementaryStream
	{
	private:
		typedef struct h264_private
		{
			struct SPS
			{
				int frame_duration;
				int cbpsize;
				int pic_order_cnt_type;
				int frame_mbs_only_flag;
				int log2_max_frame_num;
				int log2_max_pic_order_cnt_lsb;
				int delta_pic_order_always_zero_flag;
			} sps[256];

			struct PPS
			{
				int sps;
				int pic_order_present_flag;
			} pps[256];

			struct VCL_NAL
			{
				int frame_num; // slice
				int pic_parameter_set_id; // slice
				int field_pic_flag; // slice
				int bottom_field_flag; // slice
				int delta_pic_order_cnt_bottom; // slice
				int delta_pic_order_cnt_0; // slice
				int delta_pic_order_cnt_1; // slice
				int pic_order_cnt_lsb; // slice
				int idr_pic_id; // slice
				int nal_unit_type;
				int nal_ref_idc; // start code
				int pic_order_cnt_type; // sps
			} vcl_nal;

		} h264_private_t;

		typedef struct mpeg_rational_s {
			int num;
			int den;
		} mpeg_rational_t;

		enum
		{
			NAL_SLH = 0x01, // Slice Header
			NAL_SEI = 0x06, // Supplemental Enhancement Information
			NAL_SPS = 0x07, // Sequence Parameter Set
			NAL_PPS = 0x08, // Picture Parameter Set
			NAL_AUD = 0x09, // Access Unit Delimiter
			NAL_END_SEQ = 0x0A  // End of Sequence
		};

		uint32_t        m_StartCode;
		bool            m_NeedIFrame;
		bool            m_NeedSPS;
		bool            m_NeedPPS;
		int             m_Width;
		int             m_Height;
		int             m_FPS;
		int             m_FpsScale;
		mpeg_rational_t m_PixelAspect;
		int             m_FrameDuration;
		h264_private    m_streamData;
		int             m_vbvDelay;       /* -1 if CBR */
		int             m_vbvSize;        /* Video buffer size (in bytes) */
		int64_t         m_DTS;
		int64_t         m_PTS;
		bool            m_Interlaced;

	public:

		ES_h264(uint16_t pes_pid)
			: ElementaryStream(pes_pid)
		{
			m_Height = 0;
			m_Width = 0;
			m_FPS = 25;
			m_FpsScale = 0;
			m_FrameDuration = 0;
			m_vbvDelay = -1;
			m_vbvSize = 0;
			m_PixelAspect.den = 1;
			m_PixelAspect.num = 0;
			m_DTS = 0;
			m_PTS = 0;
			m_Interlaced = false;
			es_alloc_init = 1920 * 1080;
			Reset();
		}

		~ES_h264()
		{
		}

		void Parse(STREAM_PKT* pkt)
		{
			size_t frame_ptr = es_consumed;
			size_t p = es_parsed;
			uint32_t startcode = m_StartCode;
			bool frameComplete = false;

			while ((p + 3) < es_len)
			{
				if ((startcode & 0xffffff00) == 0x00000100)
				{
					if (Parse_H264(startcode, p, frameComplete) < 0)
					{
						break;
					}
				}
				startcode = startcode << 8 | es_buf[p++];
			}
			es_parsed = p;
			m_StartCode = startcode;

			if (frameComplete)
			{
				if (!m_NeedSPS && !m_NeedIFrame)
				{
					double PAR = (double)m_PixelAspect.num / (double)m_PixelAspect.den;
					double DAR = (PAR * m_Width) / m_Height;
					//DBG(DEMUX_DBG_PARSE, "H.264 SPS: PAR %i:%i\n", m_PixelAspect.num, m_PixelAspect.den);
					//DBG(DEMUX_DBG_PARSE, "H.264 SPS: DAR %.2f\n", DAR);

					uint64_t duration;
					if (c_dts != PTS_UNSET && p_dts != PTS_UNSET && c_dts > p_dts)
						duration = c_dts - p_dts;
					else
						duration = static_cast<int>(Rescale(40000, PTS_TIME_BASE, RESCALE_TIME_BASE));

					bool streamChange = false;
					if (es_frame_valid)
					{
						if (m_FpsScale == 0)
							m_FpsScale = static_cast<int>(Rescale(duration, RESCALE_TIME_BASE, PTS_TIME_BASE));
						streamChange = SetVideoInformation(m_FpsScale, RESCALE_TIME_BASE, m_Height, m_Width, static_cast<float>(DAR), m_Interlaced);
					}

					pkt->pid = pid;
					pkt->size = es_consumed - frame_ptr;
					pkt->data = &es_buf[frame_ptr];
					pkt->dts = m_DTS;
					pkt->pts = m_PTS;
					pkt->duration = duration;
					pkt->streamChange = streamChange;
				}
				m_StartCode = 0xffffffff;
				es_parsed = es_consumed;
				es_found_frame = false;
				es_frame_valid = true;
			}
		}

		void Reset()
		{
			ElementaryStream::Reset();
			m_StartCode = 0xffffffff;
			m_NeedIFrame = true;
			m_NeedSPS = true;
			m_NeedPPS = true;
			memset(&m_streamData, 0, sizeof(m_streamData));
		}

		int Parse_H264(uint32_t startcode, size_t buf_ptr, bool &complete)
		{
			size_t len = es_len - buf_ptr;
			uint8_t *buf = es_buf + buf_ptr;

			switch (startcode & 0x9f)
			{
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			{
				if (m_NeedSPS || m_NeedPPS)
				{
					es_found_frame = true;
					return 0;
				}
				// need at least 32 bytes for parsing nal
				if (len < 32)
					return -1;
				h264_private::VCL_NAL vcl;
				memset(&vcl, 0, sizeof(h264_private::VCL_NAL));
				vcl.nal_ref_idc = startcode & 0x60;
				vcl.nal_unit_type = startcode & 0x1F;
				if (!Parse_SLH(buf, len, vcl))
					return 0;

				// check for the beginning of a new access unit
				if (es_found_frame && IsFirstVclNal(vcl))
				{
					complete = true;
					es_consumed = buf_ptr - 4;
					return -1;
				}

				if (!es_found_frame)
				{
					if (buf_ptr - 4 >= (int)es_pts_pointer)
					{
						m_DTS = c_dts;
						m_PTS = c_pts;
					}
					else
					{
						m_DTS = p_dts;
						m_PTS = p_pts;
					}
				}

				m_streamData.vcl_nal = vcl;
				es_found_frame = true;
				break;
			}

			case NAL_SEI:
				if (es_found_frame)
				{
					complete = true;
					es_consumed = buf_ptr - 4;
					return -1;
				}
				break;

			case NAL_SPS:
			{
				if (es_found_frame)
				{
					complete = true;
					es_consumed = buf_ptr - 4;
					return -1;
				}
				// TODO: how big is SPS?
				if (len < 256)
					return -1;
				if (!Parse_SPS(buf, len))
					return 0;

				m_NeedSPS = false;
				break;
			}

			case NAL_PPS:
			{
				if (es_found_frame)
				{
					complete = true;
					es_consumed = buf_ptr - 4;
					return -1;
				}
				// TODO: how big is PPS
				if (len < 64)
					return -1;
				if (!Parse_PPS(buf, len))
					return 0;
				m_NeedPPS = false;
				break;
			}

			case NAL_AUD:
				if (es_found_frame && (p_pts != PTS_UNSET))
				{
					complete = true;
					es_consumed = buf_ptr - 4;
					return -1;
				}
				break;

			case NAL_END_SEQ:
				if (es_found_frame)
				{
					complete = true;
					es_consumed = buf_ptr;
					return -1;
				}
				break;

			case 13:
			case 14:
			case 15:
			case 16:
			case 17:
			case 18:
				if (es_found_frame)
				{
					complete = true;
					es_consumed = buf_ptr - 4;
					return -1;
				}
				break;

			default:
				break;
			}

			return 0;
		}

		bool Parse_PPS(uint8_t *buf, size_t len)
		{
			CBitstream bs(buf, len * 8);

			int pps_id = bs.readGolombUE();
			int sps_id = bs.readGolombUE();
			m_streamData.pps[pps_id].sps = sps_id;
			bs.readBits1();
			m_streamData.pps[pps_id].pic_order_present_flag = bs.readBits1();
			return true;
		}

		bool Parse_SLH(uint8_t *buf, size_t len, h264_private::VCL_NAL &vcl)
		{
			CBitstream bs(buf, len * 8);

			bs.readGolombUE(); /* first_mb_in_slice */
			int slice_type = bs.readGolombUE();

			if (slice_type > 4)
				slice_type -= 5;  /* Fixed slice type per frame */

			switch (slice_type)
			{
			case 0:
				break;
			case 1:
				break;
			case 2:
				m_NeedIFrame = false;
				break;
			default:
				return false;
			}

			int pps_id = bs.readGolombUE();
			int sps_id = m_streamData.pps[pps_id].sps;
			if (m_streamData.sps[sps_id].cbpsize == 0)
				return false;

			m_vbvSize = m_streamData.sps[sps_id].cbpsize;
			m_vbvDelay = -1;

			vcl.pic_parameter_set_id = pps_id;
			vcl.frame_num = bs.readBits(m_streamData.sps[sps_id].log2_max_frame_num);
			if (!m_streamData.sps[sps_id].frame_mbs_only_flag)
			{
				vcl.field_pic_flag = bs.readBits1();
				// interlaced
				if (vcl.field_pic_flag)
					m_Interlaced = true;
			}
			if (vcl.field_pic_flag)
				vcl.bottom_field_flag = bs.readBits1();

			if (vcl.nal_unit_type == 5)
				vcl.idr_pic_id = bs.readGolombUE();
			if (m_streamData.sps[sps_id].pic_order_cnt_type == 0)
			{
				vcl.pic_order_cnt_lsb = bs.readBits(m_streamData.sps[sps_id].log2_max_pic_order_cnt_lsb);
				if (m_streamData.pps[pps_id].pic_order_present_flag && !vcl.field_pic_flag)
					vcl.delta_pic_order_cnt_bottom = bs.readGolombSE();
			}
			if (m_streamData.sps[sps_id].pic_order_cnt_type == 1 &&
				!m_streamData.sps[sps_id].delta_pic_order_always_zero_flag)
			{
				vcl.delta_pic_order_cnt_0 = bs.readGolombSE();
				if (m_streamData.pps[pps_id].pic_order_present_flag && !vcl.field_pic_flag)
					vcl.delta_pic_order_cnt_1 = bs.readGolombSE();
			}

			vcl.pic_order_cnt_type = m_streamData.sps[sps_id].pic_order_cnt_type;

			return true;
		}

		bool Parse_SPS(uint8_t *buf, size_t len)
		{
			CBitstream bs(buf, len * 8);
			unsigned int tmp, frame_mbs_only;
			int cbpsize = -1;

			int profile_idc = bs.readBits(8);
			/* constraint_set0_flag = bs.readBits1();    */
			/* constraint_set1_flag = bs.readBits1();    */
			/* constraint_set2_flag = bs.readBits1();    */
			/* constraint_set3_flag = bs.readBits1();    */
			/* reserved             = bs.readBits(4);    */
			bs.skipBits(8);
			int level_idc = bs.readBits(8);
			unsigned int seq_parameter_set_id = bs.readGolombUE(9);

			unsigned int i = 0;
			while (h264_lev2cpbsize[i][0] != -1)
			{
				if (h264_lev2cpbsize[i][0] >= level_idc)
				{
					cbpsize = h264_lev2cpbsize[i][1];
					break;
				}
				i++;
			}
			if (cbpsize < 0)
				return false;

			memset(&m_streamData.sps[seq_parameter_set_id], 0, sizeof(h264_private::SPS));
			m_streamData.sps[seq_parameter_set_id].cbpsize = cbpsize * 125; /* Convert from kbit to bytes */

			if (profile_idc == 100 || profile_idc == 110 ||
				profile_idc == 122 || profile_idc == 244 || profile_idc == 44 ||
				profile_idc == 83 || profile_idc == 86 || profile_idc == 118 ||
				profile_idc == 128)
			{
				int chroma_format_idc = bs.readGolombUE(9); /* chroma_format_idc              */
				if (chroma_format_idc == 3)
					bs.skipBits(1);           /* residual_colour_transform_flag */
				bs.readGolombUE();          /* bit_depth_luma - 8             */
				bs.readGolombUE();          /* bit_depth_chroma - 8           */
				bs.skipBits(1);             /* transform_bypass               */
				if (bs.readBits1())         /* seq_scaling_matrix_present     */
				{
					for (int i = 0; i < ((chroma_format_idc != 3) ? 8 : 12); i++)
					{
						if (bs.readBits1())     /* seq_scaling_list_present       */
						{
							int last = 8, next = 8, size = (i<6) ? 16 : 64;
							for (int j = 0; j < size; j++)
							{
								if (next)
									next = (last + bs.readGolombSE()) & 0xff;
								last = !next ? last : next;
							}
						}
					}
				}
			}

			int log2_max_frame_num_minus4 = bs.readGolombUE();           /* log2_max_frame_num - 4 */
			m_streamData.sps[seq_parameter_set_id].log2_max_frame_num = log2_max_frame_num_minus4 + 4;
			int pic_order_cnt_type = bs.readGolombUE(9);
			m_streamData.sps[seq_parameter_set_id].pic_order_cnt_type = pic_order_cnt_type;
			if (pic_order_cnt_type == 0)
			{
				int log2_max_pic_order_cnt_lsb_minus4 = bs.readGolombUE();         /* log2_max_poc_lsb - 4 */
				m_streamData.sps[seq_parameter_set_id].log2_max_pic_order_cnt_lsb = log2_max_pic_order_cnt_lsb_minus4 + 4;
			}
			else if (pic_order_cnt_type == 1)
			{
				m_streamData.sps[seq_parameter_set_id].delta_pic_order_always_zero_flag = bs.readBits1();
				bs.readGolombSE();         /* offset_for_non_ref_pic          */
				bs.readGolombSE();         /* offset_for_top_to_bottom_field  */
				tmp = bs.readGolombUE();   /* num_ref_frames_in_pic_order_cnt_cycle */
				for (unsigned int i = 0; i < tmp; i++)
					bs.readGolombSE();       /* offset_for_ref_frame[i]         */
			}
			else if (pic_order_cnt_type != 2)
			{
				/* Illegal poc */
				return false;
			}

			bs.readGolombUE(9);          /* ref_frames                      */
			bs.skipBits(1);             /* gaps_in_frame_num_allowed       */
			m_Width  /* mbs */ = bs.readGolombUE() + 1;
			m_Height /* mbs */ = bs.readGolombUE() + 1;
			frame_mbs_only = bs.readBits1();
			m_streamData.sps[seq_parameter_set_id].frame_mbs_only_flag = frame_mbs_only;
			//DBG(DEMUX_DBG_PARSE, "H.264 SPS: pic_width:  %u mbs\n", (unsigned) m_Width);
			//DBG(DEMUX_DBG_PARSE, "H.264 SPS: pic_height: %u mbs\n", (unsigned) m_Height);
			//DBG(DEMUX_DBG_PARSE, "H.264 SPS: frame only flag: %d\n", frame_mbs_only);

			m_Width *= 16;
			m_Height *= 16 * (2 - frame_mbs_only);

			if (!frame_mbs_only)
			{
				bs.readBits1();
			}
			bs.skipBits(1);           /* direct_8x8_inference_flag    */
			if (bs.readBits1())       /* frame_cropping_flag */
			{
				uint32_t crop_left = bs.readGolombUE();
				uint32_t crop_right = bs.readGolombUE();
				uint32_t crop_top = bs.readGolombUE();
				uint32_t crop_bottom = bs.readGolombUE();
				//DBG(DEMUX_DBG_PARSE, "H.264 SPS: cropping %d %d %d %d\n", crop_left, crop_top, crop_right, crop_bottom);

				m_Width -= 2 * (crop_left + crop_right);
				if (frame_mbs_only)
					m_Height -= 2 * (crop_top + crop_bottom);
				else
					m_Height -= 4 * (crop_top + crop_bottom);
			}

			/* VUI parameters */
			m_PixelAspect.num = 0;
			if (bs.readBits1())    /* vui_parameters_present flag */
			{
				if (bs.readBits1())  /* aspect_ratio_info_present */
				{
					uint32_t aspect_ratio_idc = bs.readBits(8);
					//DBG(DEMUX_DBG_PARSE, "H.264 SPS: aspect_ratio_idc %d\n", aspect_ratio_idc);

					if (aspect_ratio_idc == 255 /* Extended_SAR */)
					{
						m_PixelAspect.num = bs.readBits(16); /* sar_width */
						m_PixelAspect.den = bs.readBits(16); /* sar_height */
															 //DBG(DEMUX_DBG_PARSE, "H.264 SPS: -> sar %dx%d\n", m_PixelAspect.num, m_PixelAspect.den);
					}
					else
					{
						static const mpeg_rational_t aspect_ratios[] =
						{ /* page 213: */
						  /* 0: unknown */
							{ 0, 1 },
							/* 1...16: */
							{ 1,  1 },{ 12, 11 },{ 10, 11 },{ 16, 11 },{ 40, 33 },{ 24, 11 },{ 20, 11 },{ 32, 11 },
							{ 80, 33 },{ 18, 11 },{ 15, 11 },{ 64, 33 },{ 160, 99 },{ 4,  3 },{ 3,  2 },{ 2,  1 }
						};

						if (aspect_ratio_idc < sizeof(aspect_ratios) / sizeof(aspect_ratios[0]))
						{
							memcpy(&m_PixelAspect, &aspect_ratios[aspect_ratio_idc], sizeof(mpeg_rational_t));
							//DBG(DEMUX_DBG_PARSE, "H.264 SPS: PAR %d / %d\n", m_PixelAspect.num, m_PixelAspect.den);
						}
						else
						{
							//DBG(DEMUX_DBG_PARSE, "H.264 SPS: aspect_ratio_idc out of range !\n");
						}
					}
				}
				if (bs.readBits1()) // overscan
				{
					bs.readBits1(); // overscan_appropriate_flag
				}
				if (bs.readBits1()) // video_signal_type_present_flag
				{
					bs.readBits(3); // video_format
					bs.readBits1(); // video_full_range_flag
					if (bs.readBits1()) // colour_description_present_flag
					{
						bs.readBits(8); // colour_primaries
						bs.readBits(8); // transfer_characteristics
						bs.readBits(8); // matrix_coefficients
					}
				}

				if (bs.readBits1()) // chroma_loc_info_present_flag
				{
					bs.readGolombUE(); // chroma_sample_loc_type_top_field
					bs.readGolombUE(); // chroma_sample_loc_type_bottom_field
				}

				if (bs.readBits1()) // timing_info_present_flag
				{
					//      uint32_t num_units_in_tick = bs.readBits(32);
					//      uint32_t time_scale = bs.readBits(32);
					//      int fixed_frame_rate = bs.readBits1();
					//      if (num_units_in_tick > 0)
					//        m_FPS = time_scale / (num_units_in_tick * 2);
				}
			}

			//DBG(DEMUX_DBG_PARSE, "H.264 SPS: -> video size %dx%d, aspect %d:%d\n", m_Width, m_Height, m_PixelAspect.num, m_PixelAspect.den);
			return true;
		}

		bool IsFirstVclNal(h264_private::VCL_NAL &vcl)
		{
			if (m_streamData.vcl_nal.frame_num != vcl.frame_num)
				return true;

			if (m_streamData.vcl_nal.pic_parameter_set_id != vcl.pic_parameter_set_id)
				return true;

			if (m_streamData.vcl_nal.field_pic_flag != vcl.field_pic_flag)
				return true;

			if (m_streamData.vcl_nal.field_pic_flag && vcl.field_pic_flag)
			{
				if (m_streamData.vcl_nal.bottom_field_flag != vcl.bottom_field_flag)
					return true;
			}

			if (m_streamData.vcl_nal.nal_ref_idc == 0 || vcl.nal_ref_idc == 0)
			{
				if (m_streamData.vcl_nal.nal_ref_idc != vcl.nal_ref_idc)
					return true;
			}

			if (m_streamData.vcl_nal.pic_order_cnt_type == 0 && vcl.pic_order_cnt_type == 0)
			{
				if (m_streamData.vcl_nal.pic_order_cnt_lsb != vcl.pic_order_cnt_lsb)
					return true;
				if (m_streamData.vcl_nal.delta_pic_order_cnt_bottom != vcl.delta_pic_order_cnt_bottom)
					return true;
			}

			if (m_streamData.vcl_nal.pic_order_cnt_type == 1 && vcl.pic_order_cnt_type == 1)
			{
				if (m_streamData.vcl_nal.delta_pic_order_cnt_0 != vcl.delta_pic_order_cnt_0)

					return true;
				if (m_streamData.vcl_nal.delta_pic_order_cnt_1 != vcl.delta_pic_order_cnt_1)
					return true;
			}

			if (m_streamData.vcl_nal.nal_unit_type == 5 || vcl.nal_unit_type == 5)
			{
				if (m_streamData.vcl_nal.nal_unit_type != vcl.nal_unit_type)
					return true;
			}

			if (m_streamData.vcl_nal.nal_unit_type == 5 && vcl.nal_unit_type == 5)
			{
				if (m_streamData.vcl_nal.idr_pic_id != vcl.idr_pic_id)
					return true;
			}
			return false;
		}

	};

	ElementaryStream::ElementaryStream(uint16_t pes_pid)
		: pid(pes_pid)
		, stream_type(STREAM_TYPE_UNKNOWN)
		, c_dts(PTS_UNSET)
		, c_pts(PTS_UNSET)
		, p_dts(PTS_UNSET)
		, p_pts(PTS_UNSET)
		, has_stream_info(false)
		, es_alloc_init(ES_INIT_BUFFER_SIZE)
		, es_buf(NULL)
		, es_alloc(0)
		, es_len(0)
		, es_consumed(0)
		, es_pts_pointer(0)
		, es_parsed(0)
		, es_found_frame(false)
		, es_frame_valid(false)
	{
		memset(&stream_info, 0, sizeof(STREAM_INFO));
	}

	ElementaryStream::~ElementaryStream(void)
	{
		if (es_buf)
		{
			//DBG(DEMUX_DBG_DEBUG, "free stream buffer %.4x: allocated size was %zu\n", pid, es_alloc);
			free(es_buf);
			es_buf = NULL;
		}
	}

	void ElementaryStream::Reset(void)
	{
		ClearBuffer();
		es_found_frame = false;
		es_frame_valid = false;
	}

	void ElementaryStream::ClearBuffer()
	{
		es_len = es_consumed = es_pts_pointer = es_parsed = 0;
	}

	int ElementaryStream::Append(const uint8_t* buf, size_t len, bool new_pts)
	{
		// Mark position where current pts become applicable
		if (new_pts)
			es_pts_pointer = es_len;

		if (es_buf && es_consumed)
		{
			if (es_consumed < es_len)
			{
				memmove(es_buf, es_buf + es_consumed, es_len - es_consumed);
				es_len -= es_consumed;
				es_parsed -= es_consumed;
				if (es_pts_pointer > es_consumed)
					es_pts_pointer -= es_consumed;
				else
					es_pts_pointer = 0;

				es_consumed = 0;
			}
			else
				ClearBuffer();
		}
		if (es_len + len > es_alloc)
		{
			if (es_alloc >= ES_MAX_BUFFER_SIZE)
				return -ENOMEM;

			size_t n = (es_alloc ? (es_alloc + len) * 2 : es_alloc_init);
			if (n > ES_MAX_BUFFER_SIZE)
				n = ES_MAX_BUFFER_SIZE;

			//DBG(DEMUX_DBG_DEBUG, "realloc buffer size to %zu for stream %.4x\n", n, pid);
			uint8_t* p = es_buf;
			es_buf = (uint8_t*)realloc(es_buf, n * sizeof(*es_buf));
			if (es_buf)
			{
				es_alloc = n;
			}
			else
			{
				free(p);
				es_alloc = 0;
				es_len = 0;
				return -ENOMEM;
			}
		}

		if (!es_buf)
			return -ENOMEM;

		memcpy(es_buf + es_len, buf, len);
		es_len += len;

		return 0;
	}

	const char* ElementaryStream::GetStreamCodecName(STREAM_TYPE stream_type)
	{
		switch (stream_type)
		{
		case STREAM_TYPE_VIDEO_MPEG1:
			return "mpeg1video";
		case STREAM_TYPE_VIDEO_MPEG2:
			return "mpeg2video";
		case STREAM_TYPE_AUDIO_MPEG1:
			return "mp1";
		case STREAM_TYPE_AUDIO_MPEG2:
			return "mp2";
		case STREAM_TYPE_AUDIO_AAC:
			return "aac";
		case STREAM_TYPE_AUDIO_AAC_ADTS:
			return "aac";
		case STREAM_TYPE_AUDIO_AAC_LATM:
			return "aac_latm";
		case STREAM_TYPE_VIDEO_H264:
			return "h264";
		case STREAM_TYPE_VIDEO_HEVC:
			return "hevc";
		case STREAM_TYPE_AUDIO_AC3:
			return "ac3";
		case STREAM_TYPE_AUDIO_EAC3:
			return "eac3";
		case STREAM_TYPE_DVB_TELETEXT:
			return "teletext";
		case STREAM_TYPE_DVB_SUBTITLE:
			return "dvbsub";
		case STREAM_TYPE_VIDEO_MPEG4:
			return "mpeg4video";
		case STREAM_TYPE_VIDEO_VC1:
			return "vc1";
		case STREAM_TYPE_AUDIO_LPCM:
			return "lpcm";
		case STREAM_TYPE_AUDIO_DTS:
			return "dts";
		case STREAM_TYPE_PRIVATE_DATA:
		default:
			return "data";
		}
	}

	const char* ElementaryStream::GetStreamCodecName() const
	{
		return GetStreamCodecName(stream_type);
	}

	bool ElementaryStream::GetStreamPacket(STREAM_PKT* pkt)
	{
		ResetStreamPacket(pkt);
		Parse(pkt);
		if (pkt->data)
			return true;
		return false;
	}

	void ElementaryStream::Parse(STREAM_PKT* pkt)
	{
		// No parser: pass-through
		if (es_consumed < es_len)
		{
			es_consumed = es_parsed = es_len;
			pkt->pid = pid;
			pkt->size = es_consumed;
			pkt->data = es_buf;
			pkt->dts = c_dts;
			pkt->pts = c_pts;
			if (c_dts == PTS_UNSET || p_dts == PTS_UNSET)
				pkt->duration = 0;
			else
				pkt->duration = c_dts - p_dts;
			pkt->streamChange = false;
		}
	}

	void ElementaryStream::ResetStreamPacket(STREAM_PKT* pkt)
	{
		pkt->pid = 0xffff;
		pkt->size = 0;
		pkt->data = NULL;
		pkt->dts = PTS_UNSET;
		pkt->pts = PTS_UNSET;
		pkt->duration = 0;
		pkt->streamChange = false;
	}

	uint64_t ElementaryStream::Rescale(uint64_t a, uint64_t b, uint64_t c)
	{
		uint64_t r = c / 2;

		if (b <= INT_MAX && c <= INT_MAX)
		{
			if (a <= INT_MAX)
				return (a * b + r) / c;
			else
				return a / c * b + (a % c * b + r) / c;
		}
		else
		{
			uint64_t a0 = a & 0xFFFFFFFF;
			uint64_t a1 = a >> 32;
			uint64_t b0 = b & 0xFFFFFFFF;
			uint64_t b1 = b >> 32;
			uint64_t t1 = a0 * b1 + a1 * b0;
			uint64_t t1a = t1 << 32;

			a0 = a0 * b0 + t1a;
			a1 = a1 * b1 + (t1 >> 32) + (a0 < t1a);
			a0 += r;
			a1 += a0 < r;

			for (int i = 63; i >= 0; i--)
			{
				a1 += a1 + ((a0 >> i) & 1);
				t1 += t1;
				if (c <= a1)
				{
					a1 -= c;
					t1++;
				}
			}
			return t1;
		}
	}

	bool ElementaryStream::SetVideoInformation(int FpsScale, int FpsRate, int Height, int Width, float Aspect, bool Interlaced)
	{
		bool ret = false;
		if ((stream_info.fps_scale != FpsScale) ||
			(stream_info.fps_rate != FpsRate) ||
			(stream_info.height != Height) ||
			(stream_info.width != Width) ||
			(stream_info.aspect != Aspect) ||
			(stream_info.interlaced != Interlaced))
			ret = true;

		stream_info.fps_scale = FpsScale;
		stream_info.fps_rate = FpsRate;
		stream_info.height = Height;
		stream_info.width = Width;
		stream_info.aspect = Aspect;
		stream_info.interlaced = Interlaced;

		has_stream_info = true;
		return ret;
	}

	bool ElementaryStream::SetAudioInformation(int Channels, int SampleRate, int BitRate, int BitsPerSample, int BlockAlign)
	{
		bool ret = false;
		if ((stream_info.channels != Channels) ||
			(stream_info.sample_rate != SampleRate) ||
			(stream_info.block_align != BlockAlign) ||
			(stream_info.bit_rate != BitRate) ||
			(stream_info.bits_per_sample != BitsPerSample))
			ret = true;

		stream_info.channels = Channels;
		stream_info.sample_rate = SampleRate;
		stream_info.block_align = BlockAlign;
		stream_info.bit_rate = BitRate;
		stream_info.bits_per_sample = BitsPerSample;

		has_stream_info = true;
		return ret;
	}

	AVContext::AVContext(uint64_t pos, uint16_t channel)
		: av_data_len(FLUTS_NORMAL_TS_PACKETSIZE)
		, av_pkt_size(188)
		, m_channel(channel)
		, pid(0xffff)
		, transport_error(false)
		, has_payload(false)
		, payload_unit_start(false)
		, discontinuity(false)
		, payload(NULL)
		, payload_len(0)
		, packet(NULL)
	{

		memset(av_buf, 0, sizeof(av_buf));
	};

	void AVContext::Reset(void)
	{
		pid = 0xffff;
		transport_error = false;
		has_payload = false;
		payload_unit_start = false;
		discontinuity = false;
		payload = NULL;
		payload_len = 0;
		packet = NULL;
	}

	uint16_t AVContext::GetPID() const
	{
		return pid;
	}

	PACKET_TYPE AVContext::GetPIDType() const
	{
		if (packet)
			return packet->packet_type;
		return PACKET_TYPE_UNKNOWN;
	}

	uint16_t AVContext::GetPIDChannel() const
	{
		if (packet)
			return packet->channel;
		return 0xffff;
	}

	bool AVContext::HasPIDStreamData() const
	{
		// PES packets append frame buffer of elementary stream until next start of unit
		// On new unit start, flag is held
		if (packet && packet->has_stream_data)
			return true;
		return false;
	}

	bool AVContext::HasPIDPayload() const
	{
		return has_payload;
	}

	ElementaryStream* AVContext::GetPIDStream()
	{
		if (packet && packet->packet_type == PACKET_TYPE_PES)
			return packet->stream;
		return NULL;
	}

	std::vector<ElementaryStream*> AVContext::GetStreams()
	{
		std::vector<ElementaryStream*> v;
		for (std::map<uint16_t, Packet>::iterator it = packets.begin(); it != packets.end(); ++it)
			if (it->second.packet_type == PACKET_TYPE_PES && it->second.stream)
				v.push_back(it->second.stream);
		return v;
	}

	void AVContext::StartStreaming(uint16_t pid)
	{
		std::map<uint16_t, Packet>::iterator it = packets.find(pid);
		if (it != packets.end())
			it->second.streaming = true;
	}

	void AVContext::StopStreaming(uint16_t pid)
	{
		std::map<uint16_t, Packet>::iterator it = packets.find(pid);
		if (it != packets.end())
			it->second.streaming = false;
	}

	ElementaryStream* AVContext::GetStream(uint16_t pid) const
	{
		std::map<uint16_t, Packet>::const_iterator it = packets.find(pid);
		if (it != packets.end())
			return it->second.stream;
		return NULL;
	}

	uint16_t AVContext::GetChannel(uint16_t pid) const
	{
		std::map<uint16_t, Packet>::const_iterator it = packets.find(pid);
		if (it != packets.end())
			return it->second.channel;
		return 0xffff;
	}

	void AVContext::ResetPackets()
	{
		for (std::map<uint16_t, Packet>::iterator it = packets.begin(); it != packets.end(); ++it)
		{
			it->second.Reset();
		}
	}

	uint8_t AVContext::av_rb8(const uint8_t* p)
	{
		uint8_t val = *(uint8_t*)p;
		return val;
	}

	uint16_t AVContext::av_rb16(const uint8_t* p)
	{
		uint16_t val = av_rb8(p) << 8;
		val |= av_rb8(p + 1);
		return val;
	}

	uint32_t AVContext::av_rb32(const uint8_t* p)
	{
		uint32_t val = av_rb16(p) << 16;
		val |= av_rb16(p + 2);
		return val;
	}

	uint64_t AVContext::decode_pts(const uint8_t* p)
	{
		uint64_t pts = (uint64_t)(av_rb8(p) & 0x0e) << 29 | (av_rb16(p + 1) >> 1) << 15 | av_rb16(p + 3) >> 1;
		return pts;
	}

	STREAM_TYPE AVContext::get_stream_type(uint8_t pes_type)
	{
		switch (pes_type)
		{
		case 0x01:
			return STREAM_TYPE_VIDEO_MPEG1;
		case 0x02:
			return STREAM_TYPE_VIDEO_MPEG2;
		case 0x03:
			return STREAM_TYPE_AUDIO_MPEG1;
		case 0x04:
			return STREAM_TYPE_AUDIO_MPEG2;
		case 0x06:
			return STREAM_TYPE_PRIVATE_DATA;
		case 0x0f:
		case 0x11:
			return STREAM_TYPE_AUDIO_AAC;
		case 0x10:
			return STREAM_TYPE_VIDEO_MPEG4;
		case 0x1b:
			return STREAM_TYPE_VIDEO_H264;
		case 0x24:
			return STREAM_TYPE_VIDEO_HEVC;
		case 0xea:
			return STREAM_TYPE_VIDEO_VC1;
		case 0x80:
			return STREAM_TYPE_AUDIO_LPCM;
		case 0x81:
		case 0x83:
		case 0x84:
		case 0x87:
			return STREAM_TYPE_AUDIO_AC3;
		case 0x82:
		case 0x85:
		case 0x8a:
			return STREAM_TYPE_AUDIO_DTS;
		}
		return STREAM_TYPE_UNKNOWN;
	}

	int AVContext::TSResync(uint8_t* buf, int buf_size)
	{
		if (buf[0] == 0x47)
		{
			memcpy(av_buf, buf, buf_size);
			Reset();
			return AVCONTEXT_CONTINUE;
		}
		return AVCONTEXT_TS_NOSYNC;
	}

	uint64_t AVContext::GoNext()
	{
		Reset();
		return 0;
	}

	uint64_t AVContext::Shift()
	{
		Reset();
		return 0;
	}

	int AVContext::ProcessTSPacket()
	{
		int ret = AVCONTEXT_CONTINUE;
		std::map<uint16_t, Packet>::iterator it;

		if (av_rb8(this->av_buf) != 0x47) // ts sync byte
			return AVCONTEXT_TS_NOSYNC;

		uint16_t header = av_rb16(this->av_buf + 1);
		this->pid = header & 0x1fff;
		this->transport_error = (header & 0x8000) != 0;
		this->payload_unit_start = (header & 0x4000) != 0;
		// Cleaning context
		this->discontinuity = false;
		this->has_payload = false;
		this->payload = NULL;
		this->payload_len = 0;

		if (this->transport_error)
			return AVCONTEXT_CONTINUE;
		// Null packet
		if (this->pid == 0x1fff)
			return AVCONTEXT_CONTINUE;

		uint8_t flags = av_rb8(this->av_buf + 3);
		bool has_payload = (flags & 0x10) != 0;
		bool is_discontinuity = false;
		uint8_t continuity_counter = flags & 0x0f;
		bool has_adaptation = (flags & 0x20) != 0;
		size_t n = 0;
		if (has_adaptation)
		{
			size_t len = (size_t)av_rb8(this->av_buf + 4);
			if (len > (this->av_data_len - 5))
			{
				return AVCONTEXT_TS_ERROR;
			}
			n = len + 1;
			if (len > 0)
			{
				is_discontinuity = (av_rb8(this->av_buf + 5) & 0x80) != 0;
			}
		}
		if (has_payload)
		{
			// Payload start after adaptation fields
			this->payload = this->av_buf + n + 4;
			this->payload_len = this->av_data_len - n - 4;
		}

		it = this->packets.find(this->pid);
		if (it == this->packets.end())
		{
			// Not registred PID
			// We are waiting for unit start of PID 0 else next packet is required
			if (this->pid == 0 && this->payload_unit_start)
			{
				// Registering PID 0
				Packet pid0;
				pid0.pid = this->pid;
				pid0.packet_type = PACKET_TYPE_PSI;
				pid0.continuity = continuity_counter;
				it = this->packets.insert(it, std::make_pair(this->pid, pid0));
			}
			else
				return AVCONTEXT_CONTINUE;
		}
		else
		{
			// PID is registred
			// Checking unit start is required
			if (it->second.wait_unit_start && !this->payload_unit_start)
			{
				// Not unit start. Save packet flow continuity...
				it->second.continuity = continuity_counter;
				this->discontinuity = true;
				return AVCONTEXT_DISCONTINUITY;
			}
			// Checking continuity where possible
			if (it->second.continuity != 0xff)
			{
				uint8_t expected_cc = has_payload ? (it->second.continuity + 1) & 0x0f : it->second.continuity;
				if (!is_discontinuity && expected_cc != continuity_counter)
				{
					this->discontinuity = true;
					// If unit is not start then reset PID and wait the next unit start
					if (!this->payload_unit_start)
					{
						it->second.Reset();
						//DBG(DEMUX_DBG_WARN, "PID %.4x discontinuity detected: found %u, expected %u\n", this->pid, continuity_counter, expected_cc);
						return AVCONTEXT_DISCONTINUITY;
					}
				}
			}
			it->second.continuity = continuity_counter;
		}

		this->discontinuity |= is_discontinuity;
		this->has_payload = has_payload;
		this->packet = &(it->second);

		// It is time to stream data for PES
		if (this->payload_unit_start &&
			this->packet->streaming &&
			this->packet->packet_type == PACKET_TYPE_PES &&
			!this->packet->wait_unit_start)
		{
			this->packet->has_stream_data = true;
			ret = AVCONTEXT_STREAM_PID_DATA;
		}
		return ret;
	}

	int AVContext::ProcessTSPayload()
	{
		if (!this->packet)
			return AVCONTEXT_CONTINUE;

		int ret = 0;
		switch (this->packet->packet_type)
		{
		case PACKET_TYPE_PSI:
			ret = parse_ts_psi();
			break;
		case PACKET_TYPE_PES:
			ret = parse_ts_pes();
			break;
		case PACKET_TYPE_UNKNOWN:
			break;
		}

		return ret;
	}

	void AVContext::clear_pmt()
	{
		//DBG(DEMUX_DBG_DEBUG, "%s\n", __FUNCTION__);
		std::vector<uint16_t> pid_list;
		for (std::map<uint16_t, Packet>::iterator it = this->packets.begin(); it != this->packets.end(); ++it)
		{
			if (it->second.packet_type == PACKET_TYPE_PSI && it->second.packet_table.table_id == 0x02)
			{
				pid_list.push_back(it->first);
				clear_pes(it->second.channel);
			}
		}
		for (std::vector<uint16_t>::iterator it = pid_list.begin(); it != pid_list.end(); ++it)
			this->packets.erase(*it);
	}

	void AVContext::clear_pes(uint16_t channel)
	{
		//DBG(DEMUX_DBG_DEBUG, "%s(%u)\n", __FUNCTION__, channel);
		std::vector<uint16_t> pid_list;
		for (std::map<uint16_t, Packet>::iterator it = this->packets.begin(); it != this->packets.end(); ++it)
		{
			if (it->second.packet_type == PACKET_TYPE_PES && it->second.channel == channel)
				pid_list.push_back(it->first);
		}
		for (std::vector<uint16_t>::iterator it = pid_list.begin(); it != pid_list.end(); ++it)
			this->packets.erase(*it);
	}

	int AVContext::parse_ts_psi()
	{
		size_t len;

		if (!this->has_payload || !this->payload || !this->payload_len || !this->packet)
			return AVCONTEXT_CONTINUE;

		if (this->payload_unit_start)
		{
			// Reset wait for unit start
			this->packet->wait_unit_start = false;
			// pointer field present
			len = (size_t)av_rb8(this->payload);
			if (len > this->payload_len)
			{
				return AVCONTEXT_TS_ERROR;
			}

			// table ID
			uint8_t table_id = av_rb8(this->payload + 1);

			// table length
			len = (size_t)av_rb16(this->payload + 2);
			if ((len & 0x3000) != 0x3000)
			{
#if defined(TSDEMUX_DEBUG)
				assert(false);
#else
				return AVCONTEXT_TS_ERROR;
#endif
			}
			len &= 0x0fff;

			this->packet->packet_table.Reset();

			size_t n = this->payload_len - 4;
			memcpy(this->packet->packet_table.buf, this->payload + 4, n);
			this->packet->packet_table.table_id = table_id;
			this->packet->packet_table.offset = (uint16_t)n;
			this->packet->packet_table.len = (uint16_t)len;
			// check for incomplete section
			if (this->packet->packet_table.offset < this->packet->packet_table.len)
				return AVCONTEXT_CONTINUE;
		}
		else
		{
			// next part of PSI
			if (this->packet->packet_table.offset == 0)
			{
				return AVCONTEXT_TS_ERROR;
			}

			if ((this->payload_len + this->packet->packet_table.offset) > TABLE_BUFFER_SIZE)
			{
				return AVCONTEXT_TS_ERROR;
			}
			memcpy(this->packet->packet_table.buf + this->packet->packet_table.offset, this->payload, this->payload_len);
			this->packet->packet_table.offset += (uint16_t)this->payload_len;
			// check for incomplete section
			if (this->packet->packet_table.offset < this->packet->packet_table.len)
				return AVCONTEXT_CONTINUE;
		}

		// now entire table is filled
		const uint8_t* psi = this->packet->packet_table.buf;
		const uint8_t* end_psi = psi + this->packet->packet_table.len;

		switch (this->packet->packet_table.table_id)
		{
		case 0x00: // parse PAT table
		{
			// check if version number changed
			uint16_t id = av_rb16(psi);
			// check if applicable
			if ((av_rb8(psi + 2) & 0x01) == 0)
				return AVCONTEXT_CONTINUE;
			// check if version number changed
			uint8_t version = (av_rb8(psi + 2) & 0x3e) >> 1;
			if (id == this->packet->packet_table.id && version == this->packet->packet_table.version)
				return AVCONTEXT_CONTINUE;
			//DBG(DEMUX_DBG_DEBUG, "%s: new PAT version %u\n", __FUNCTION__, version);

			// clear old associated pmt
			clear_pmt();

			// parse new version of PAT
			psi += 5;

			end_psi -= 4; // CRC32

			if (psi >= end_psi)
			{
				return AVCONTEXT_TS_ERROR;
			}

			len = end_psi - psi;

			if (len % 4)
			{
				return AVCONTEXT_TS_ERROR;
			}

			size_t n = len / 4;

			for (size_t i = 0; i < n; i++, psi += 4)
			{
				uint16_t channel = av_rb16(psi);
				uint16_t pmt_pid = av_rb16(psi + 2);

				pmt_pid &= 0x1fff;

				//DBG(DEMUX_DBG_DEBUG, "%s: PAT version %u: new PMT %.4x channel %u\n", __FUNCTION__, version, pmt_pid, channel);
				if (this->m_channel == 0 || this->m_channel == channel)
				{
					Packet& pmt = this->packets[pmt_pid];
					pmt.pid = pmt_pid;
					pmt.packet_type = PACKET_TYPE_PSI;
					pmt.channel = channel;
				}
			}
			// PAT is processed. New version is available
			this->packet->packet_table.id = id;
			this->packet->packet_table.version = version;
			break;
		}
		case 0x02: // parse PMT table
		{
			uint16_t id = av_rb16(psi);
			// check if applicable
			if ((av_rb8(psi + 2) & 0x01) == 0)
				return AVCONTEXT_CONTINUE;
			// check if version number changed
			uint8_t version = (av_rb8(psi + 2) & 0x3e) >> 1;
			if (id == this->packet->packet_table.id && version == this->packet->packet_table.version)
				return AVCONTEXT_CONTINUE;
			//DBG(DEMUX_DBG_DEBUG, "%s: PMT(%.4x) version %u\n", __FUNCTION__, this->packet->pid, version);

			// clear old pes
			clear_pes(this->packet->channel);

			// parse new version of PMT
			psi += 7;

			end_psi -= 4; // CRC32

			if (psi >= end_psi)
			{
				return AVCONTEXT_TS_ERROR;
			}

			len = (size_t)(av_rb16(psi) & 0x0fff);
			psi += 2 + len;

			while (psi < end_psi)
			{
				if (end_psi - psi < 5)
				{
					return AVCONTEXT_TS_ERROR;
				}

				uint8_t pes_type = av_rb8(psi);
				uint16_t pes_pid = av_rb16(psi + 1);

				// Reserved fields in table sections must be "set" to '1' bits.
				//if ((pes_pid & 0xe000) != 0xe000)
				//  return AVCONTEXT_TS_ERROR;

				pes_pid &= 0x1fff;

				// len of descriptor section
				len = (size_t)(av_rb16(psi + 3) & 0x0fff);
				psi += 5;

				// ignore unknown streams
				STREAM_TYPE stream_type = get_stream_type(pes_type);

				if (stream_type != STREAM_TYPE_UNKNOWN)
				{
					Packet& pes = this->packets[pes_pid];
					pes.pid = pes_pid;
					pes.packet_type = PACKET_TYPE_PES;
					pes.channel = this->packet->channel;
					// Disable streaming by default
					pes.streaming = false;
					// Get basic stream infos from PMT table
					STREAM_INFO stream_info;
					stream_info = parse_pes_descriptor(psi, len, &stream_type);

					ElementaryStream* es = NULL;
					switch (stream_type)
					{
					case STREAM_TYPE_AUDIO_AAC:
					case STREAM_TYPE_AUDIO_AAC_ADTS:
					case STREAM_TYPE_AUDIO_AAC_LATM:
						es = new ES_AAC(pes_pid);
						m_pidAAC = pes_pid;
						printf("AAC PID = %d\r\n", m_pidAAC);
						break;
					case STREAM_TYPE_VIDEO_H264:
						es = new ES_h264(pes_pid);
						m_pidH264 = pes_pid;
						printf("H264 PID = %d\r\n", m_pidH264);
						break;
					default:
						// No parser: pass-through
						es = new ElementaryStream(pes_pid);
						es->has_stream_info = true;
						break;
					}

					es->stream_type = stream_type;
					es->stream_info = stream_info;
					pes.stream = es;
				}
				psi += len;
			}

			if (psi != end_psi)
			{
				return AVCONTEXT_TS_ERROR;
			}

			// PMT is processed. New version is available
			this->packet->packet_table.id = id;
			this->packet->packet_table.version = version;
			return AVCONTEXT_PROGRAM_CHANGE;
		}
		default:
			// CAT, NIT table
			break;
		}

		return AVCONTEXT_CONTINUE;
	}

	STREAM_INFO AVContext::parse_pes_descriptor(const uint8_t* p, size_t len, STREAM_TYPE* st)
	{
		const uint8_t* desc_end = p + len;
		STREAM_INFO si;
		memset(&si, 0, sizeof(STREAM_INFO));

		while (p < desc_end)
		{
			uint8_t desc_tag = av_rb8(p);
			uint8_t desc_len = av_rb8(p + 1);
			p += 2;
			//DBG(DEMUX_DBG_DEBUG, "%s: tag %.2x len %d\n", __FUNCTION__, desc_tag, desc_len);
			switch (desc_tag)
			{
			case 0x02:
			case 0x03:
				break;
			case 0x0a: /* ISO 639 language descriptor */
				if (desc_len >= 4)
				{
					si.language[0] = av_rb8(p);
					si.language[1] = av_rb8(p + 1);
					si.language[2] = av_rb8(p + 2);
					si.language[3] = 0;
				}
				break;
			case 0x56: /* DVB teletext descriptor */
				*st = STREAM_TYPE_DVB_TELETEXT;
				break;
			case 0x6a: /* DVB AC3 */
			case 0x81: /* AC3 audio stream */
				*st = STREAM_TYPE_AUDIO_AC3;
				break;
			case 0x7a: /* DVB enhanced AC3 */
				*st = STREAM_TYPE_AUDIO_EAC3;
				break;
			case 0x7b: /* DVB DTS */
				*st = STREAM_TYPE_AUDIO_DTS;
				break;
			case 0x7c: /* DVB AAC */
				*st = STREAM_TYPE_AUDIO_AAC;
				break;
			case 0x59: /* subtitling descriptor */
				if (desc_len >= 8)
				{
					/*
					* Byte 4 is the subtitling_type field
					* av_rb8(p + 3) & 0x10 : normal
					* av_rb8(p + 3) & 0x20 : for the hard of hearing
					*/
					*st = STREAM_TYPE_DVB_SUBTITLE;
					si.language[0] = av_rb8(p);
					si.language[1] = av_rb8(p + 1);
					si.language[2] = av_rb8(p + 2);
					si.language[3] = 0;
					si.composition_id = (int)av_rb16(p + 4);
					si.ancillary_id = (int)av_rb16(p + 6);
				}
				break;
			case 0x05: /* registration descriptor */
			case 0x1E: /* SL descriptor */
			case 0x1F: /* FMC descriptor */
			case 0x52: /* stream identifier descriptor */
			default:
				break;
			}
			p += desc_len;
		}

		return si;
	}

	int AVContext::parse_ts_pes()
	{
		if (!this->has_payload || !this->payload || !this->payload_len || !this->packet)
			return AVCONTEXT_CONTINUE;

		if (!this->packet->stream)
			return AVCONTEXT_CONTINUE;

		if (this->payload_unit_start)
		{
			// Wait for unit start: Reset frame buffer to clear old data
			if (this->packet->wait_unit_start)
			{
				packet->stream->Reset();
				packet->stream->p_dts = PTS_UNSET;
				packet->stream->p_pts = PTS_UNSET;
			}
			this->packet->wait_unit_start = false;
			this->packet->has_stream_data = false;
			// Reset header table
			this->packet->packet_table.Reset();
			// Header len is at least 6 bytes. So getting 6 bytes first
			this->packet->packet_table.len = 6;
		}

		// Position in the payload buffer. Start at 0
		size_t pos = 0;

		while (this->packet->packet_table.offset < this->packet->packet_table.len)
		{
			if (pos >= this->payload_len)
				return AVCONTEXT_CONTINUE;

			size_t n = this->packet->packet_table.len - this->packet->packet_table.offset;

			if (n >(this->payload_len - pos))
				n = this->payload_len - pos;

			memcpy(this->packet->packet_table.buf + this->packet->packet_table.offset, this->payload + pos, n);
			this->packet->packet_table.offset += (uint16_t)n;
			pos += n;

			if (this->packet->packet_table.offset == 6)
			{
				if (memcmp(this->packet->packet_table.buf, "\x00\x00\x01", 3) == 0)
				{
					uint8_t stream_id = av_rb8(this->packet->packet_table.buf + 3);
					if (stream_id == 0xbd || (stream_id >= 0xc0 && stream_id <= 0xef))
						this->packet->packet_table.len = 9;
				}
			}
			else if (this->packet->packet_table.offset == 9)
			{
				this->packet->packet_table.len += av_rb8(this->packet->packet_table.buf + 8);
			}
		}

		// parse header table
		bool has_pts = false;

		if (this->packet->packet_table.len >= 9)
		{
			uint8_t flags = av_rb8(this->packet->packet_table.buf + 7);

			//this->packet->stream->frame_num++;

			switch (flags & 0xc0)
			{
			case 0x80: // PTS only
			{
				has_pts = true;
				if (this->packet->packet_table.len >= 14)
				{
					uint64_t pts = decode_pts(this->packet->packet_table.buf + 9);
					this->packet->stream->p_dts = this->packet->stream->c_dts;
					this->packet->stream->p_pts = this->packet->stream->c_pts;
					this->packet->stream->c_dts = this->packet->stream->c_pts = pts;
				}
				else
				{
					this->packet->stream->c_dts = this->packet->stream->c_pts = PTS_UNSET;
				}
			}
			break;
			case 0xc0: // PTS,DTS
			{
				has_pts = true;
				if (this->packet->packet_table.len >= 19)
				{
					uint64_t pts = decode_pts(this->packet->packet_table.buf + 9);
					uint64_t dts = decode_pts(this->packet->packet_table.buf + 14);
					int64_t d = (pts - dts) & PTS_MASK;
					// more than two seconds of PTS/DTS delta, probably corrupt
					if (d > 180000)
					{
						this->packet->stream->c_dts = this->packet->stream->c_pts = PTS_UNSET;
					}
					else
					{
						this->packet->stream->p_dts = this->packet->stream->c_dts;
						this->packet->stream->p_pts = this->packet->stream->c_pts;
						this->packet->stream->c_dts = dts;
						this->packet->stream->c_pts = pts;
					}
				}
				else
				{
					this->packet->stream->c_dts = this->packet->stream->c_pts = PTS_UNSET;
				}
			}
			break;
			}
			this->packet->packet_table.Reset();
		}

		if (this->packet->streaming)
		{
			const uint8_t* data = this->payload + pos;
			size_t len = this->payload_len - pos;
			this->packet->stream->Append(data, len, has_pts);
		}

		return AVCONTEXT_CONTINUE;
	}
}

class WXTsDemux
{
public:
	WXTsDemux()
	{
		m_mainStreamPID = 0xffff;
		m_DTS = PTS_UNSET;
		m_PTS = PTS_UNSET;
		m_pinTime = m_curTime = m_endTime = 0;
		m_AVContext = new TSDemux::AVContext(0, 0);

		m_aacBuf = new uint8_t[8192];
		m_nAudioSize = 0;

		m_h264Buf = new uint8_t[1920 * 1080];
		m_nVideoSize = 0;
	}

	virtual ~WXTsDemux()
	{
		// Free AV context
		if (m_AVContext)
			delete m_AVContext;
		if (m_aacBuf)
			delete m_aacBuf;
		if (m_h264Buf)
			delete m_h264Buf;
	}

	uint8_t *m_aacBuf = nullptr;
	size_t m_nAudioSize = 0;

	uint8_t *m_h264Buf = nullptr;
	size_t m_nVideoSize = 0;
	//188 TS
	int  ProcessTS(const uint8_t* buf, int buf_size)
	{
		int result = 0;
		int ret = m_AVContext->TSResync((uint8_t*)buf, buf_size);  //解碼
		if (ret != TSDemux::AVCONTEXT_CONTINUE)
			return TSDemux::AVCONTEXT_TS_ERROR;

		ret = m_AVContext->ProcessTSPacket(); //TSDemux 處理

		if (m_AVContext->HasPIDStreamData())
		{
			TSDemux::STREAM_PKT pkt;
			if (get_stream_data(&pkt))
			{
				if (pkt.size > 0 && pkt.data)
				{
					if (pkt.pid == m_AVContext->m_pidAAC) {
						m_nAudioSize = pkt.size;
						memcpy(m_aacBuf, pkt.data, pkt.size);
						result = 2;  //AUDIO
					}
					else 	if (pkt.pid == m_AVContext->m_pidH264) {
						ParseExtraData(pkt.data, pkt.size);
						m_nVideoSize = pkt.size;
						memcpy(m_h264Buf, pkt.data, pkt.size);
						result = 1;
					}
				}
			}
		}

		if (m_AVContext->HasPIDPayload()) {
			ret = m_AVContext->ProcessTSPayload();
			if (ret == TSDemux::AVCONTEXT_PROGRAM_CHANGE) {
				const std::vector<TSDemux::ElementaryStream*> es_streams = m_AVContext->GetStreams();
				if (!es_streams.empty())
				{
					m_mainStreamPID = es_streams[0]->pid;
					for (auto it = es_streams.begin(); it != es_streams.end(); ++it)
					{
						m_AVContext->StartStreaming((*it)->pid);
					}
				}
			}
		}

		if (ret == TSDemux::AVCONTEXT_TS_ERROR)
			m_AVContext->Shift();
		else
			m_AVContext->GoNext();

		return result;
	}
	void   GetExtraData(uint8_t*&buf, int &buf_size) {
		buf = m_extradata;
		buf_size = m_extrasize;
	}
	void   GetVideoData(uint8_t *&buf, int &buf_size) {
		buf = m_h264Buf;
		buf_size = (int)m_nVideoSize;
	}
	void   GetAudioData(uint8_t *&buf, int &buf_size) {
		buf = m_aacBuf;
		buf_size = (int)m_nAudioSize;
	}
private:
	bool get_stream_data(TSDemux::STREAM_PKT* pkt)
	{
		TSDemux::ElementaryStream* es = m_AVContext->GetPIDStream();
		if (!es)
			return false;

		if (!es->GetStreamPacket(pkt))
			return false;

		if (pkt->duration > 180000)
		{
			pkt->duration = 0;
		}
		else if (pkt->pid == m_mainStreamPID)
		{
			// Fill duration map for main stream
			m_curTime += pkt->duration;
			if (m_curTime >= m_pinTime)
			{
				m_pinTime += POSMAP_PTS_INTERVAL;
				if (m_curTime > m_endTime)
				{
					m_endTime = m_curTime;
				}
			}
			// Sync main DTS & PTS
			m_DTS = pkt->dts;
			m_PTS = pkt->pts;
		}
		return true;
	}


	bool m_bExtra = false;//
	uint8_t m_extradata[200];
	int m_extrasize = 0;
	//从NAL码流解析出extradata(SPS/PPS)
	bool ParseExtraData(const uint8_t *buf, size_t buf_size) {
		if (m_bExtra) {
			return false;
		}
		std::vector<int> arrPos;//位置
		std::vector<int> arrType;//类型
		std::vector<int> arrLength;//长度

		bool FindSPS = false;
		bool FindPPS = false;
		int  sps_pos = 0;
		int  pps_pos = 0;
		int  pos = 0;
		int  type = 0;
		int  lastPos = 0;
		while (pos < buf_size - 4) {
			if (buf[pos] == 0 && buf[pos + 1] == 0 && buf[pos + 2] == 0 && buf[pos + 3] == 1) {
				type = buf[pos + 4] & 0x1F;
				arrPos.push_back(pos);
				arrType.push_back(type);

				int length = pos - lastPos;
				if (length != 0)
					arrLength.push_back(length);
				lastPos = pos;
				if (type == 7) {
					FindSPS = true;
					sps_pos = pos;
				}
				else if (type == 8) {
					FindPPS = true;
					pps_pos = pos;
				}
				pos += 4;
				continue;
			}if (buf[pos] == 0 && buf[pos + 1] == 0 && buf[pos + 2] == 1) {
				type = buf[pos + 3] & 0x1F;
				arrPos.push_back(pos);
				arrType.push_back(type);

				int length = pos - lastPos;
				if (length != 0)
					arrLength.push_back(length);
				lastPos = pos;
				if (type == 7) {
					FindSPS = true;
					sps_pos = pos;
				}
				else if (type == 8) {
					FindPPS = true;
					pps_pos = pos;
				}
				pos += 4;
				continue;
			}
			else {
				pos++;
				continue;
			}
		}
		int length = pos - lastPos;
		arrLength.push_back(length);
		if (FindSPS && FindPPS) {
			m_bExtra = true;
			int  sps_len = 0;
			int  pps_len = 0;
			for (int i = 0; i < arrType.size(); i++) {
				if (arrType[i] == 7) {
					sps_len = arrLength[i];
				}
				if (arrType[i] == 8) {
					pps_len = arrLength[i];
				}
			}

			memset(m_extradata, 0, 200);
			m_extrasize = sps_len + pps_len;
			memcpy(m_extradata, buf + sps_pos, sps_len);
			memcpy(m_extradata + sps_len, buf + pps_pos, pps_len);
			return true;
		}
		return false;
	}
private:
	// Playback context
	TSDemux::AVContext* m_AVContext = NULL;
	uint16_t m_mainStreamPID;     ///< PID of main stream
	uint64_t m_DTS;               ///< absolute decode time of main stream
	uint64_t m_PTS;               ///< absolute presentation time of main stream
	int64_t m_pinTime;            ///< pinned relative position (90Khz)
	int64_t m_curTime;            ///< current relative position (90Khz)
	int64_t m_endTime;            ///< last relative marked position (90Khz))
};

WXMEDIA_API void* TSDemuxCreate() {
	WXTsDemux *dex = new WXTsDemux();
	return (void*)dex;
}

WXMEDIA_API void  TSDemuxDestroy(void *ptr) {
	if (ptr) {
		WXTsDemux *dex = (WXTsDemux *)ptr;
		delete dex;
	}
}

WXMEDIA_API int  TSDemuxWriteData(void *ptr, const uint8_t *buf, int size) {
	if (ptr) {
		WXTsDemux *dex = (WXTsDemux *)ptr;
		return dex->ProcessTS(buf, size);
	}
	return 0;
}

WXMEDIA_API void   TSDemuxGetExtraData(void *ptr, uint8_t **buf, int *buf_size) {
	if (ptr) {
		WXTsDemux *dex = (WXTsDemux *)ptr;
		dex->GetExtraData(*buf, *buf_size);
	}
}

WXMEDIA_API void   TSDemuxGetVideoData(void *ptr, uint8_t **buf, int *buf_size) {
	if (ptr) {
		WXTsDemux *dex = (WXTsDemux *)ptr;
		dex->GetVideoData(*buf, *buf_size);
	}
}

WXMEDIA_API void   TSDemuxGetAudioData(void *ptr, uint8_t **buf, int *buf_size) {
	if (ptr) {
		WXTsDemux *dex = (WXTsDemux *)ptr;
		dex->GetAudioData(*buf, *buf_size);
	}
}