#include "camerav4l2.h"

// storage for static arrays
unsigned char CameraV4L2::sRGBVal8[1024];
unsigned short CameraV4L2::sRGBVal16[1024];

#define CLIP8(x) ((x) < 0 ? 0 : ((x) >= 255 ? 255 : (x)))
#define CLIP16(x) ((x) < 0 ? 0 : ((x) >= 65535 ? 65535 : (x)))

CameraV4L2::CameraV4L2(std::string device, int inputRange)
{
//	m_caps = {};
//    m_fmt = {0};
	float IRange = (float)inputRange;
	m_DeviceName = device;
	m_Width = 672; m_Height = 380;
	m_fd = open(device.data(), O_RDWR);
	if(m_fd <= 0)
		fprintf(stderr,"Error: Unable to open Device");
	// fill the sRGB arrays
	float fI;
	float a = 0.055F;
	float recipGamma = 1.0F / 2.4F;
	float rangeVal;
	unsigned short SVal;
	unsigned char  CVal;
	for (int i = 0; i < 1024; i++)
	{
		fI = (float)i / IRange;
		if (fI < 0.0031308F)
		{
			rangeVal = 12.92F * fI;
		}
		else
		{
			rangeVal = ((1.0 - a) * pow(fI, recipGamma) - a);
		}
		// saturate table values just in case
		SVal = (unsigned short)CLIP16((int)(65536 * rangeVal));
		CVal = (unsigned char)CLIP8((int)(256 * rangeVal));
		sRGBVal16[i] = SVal;
		sRGBVal8[i] = CVal;
	}
}
CameraV4L2::~CameraV4L2()
{
	if(m_fd > 0)
		close(m_fd);
}
bool CameraV4L2::Exists()
{
	return (m_fd > 0);
}
CameraV4L2::ERR CameraV4L2::SetSize(int width, int height)
{
	m_Width = width;
	m_Height = height;
}
CameraV4L2::ERR CameraV4L2::SetFormat(struct v4l2_format &fmt)
{      
	if (-1 == xioctl(VIDIOC_S_FMT, &fmt))
	{
		fprintf(stderr,"Error: Setting Pixel Format");
		return FAIL;
	}
	return OK;
}
CameraV4L2::ERR CameraV4L2::GetFormat(struct v4l2_format &fmt)
{      
    if (-1 == xioctl(VIDIOC_G_FMT, &fmt))
    {
        fprintf(stderr,"Error: Getting Pixel Format");
        return FAIL;
    }
    return OK;
}
int CameraV4L2::GetExposure()	// in 1/10 ms
{
    struct v4l2_control c;
    c.id = 10094850;	// exposure
    if (-1 == xioctl(VIDIOC_G_CTRL, &c))
    {
        fprintf(stderr,"Error: Getting Exposure");
	    return -1;
    }
    return c.value;
}
CameraV4L2::ERR CameraV4L2::SetExposure(int tenth_ms)
{
    struct v4l2_control c;
    c.id = 10094850;	// exposure
	c.value = tenth_ms;
    if (-1 == xioctl(VIDIOC_S_CTRL, &c))
    {
        fprintf(stderr,"Error: Setting Exposure");
        return FAIL;
    }
	else
		fprintf(stderr,"Set Exposure %6.1f ms\n",(float)c.value/10.0);
    return OK;
}
int CameraV4L2::GetBrightness()	// in ms
{
    struct v4l2_control c;
    c.id = 9963776;	// brightness
    if (-1 == xioctl(VIDIOC_G_CTRL, &c))
    {
        fprintf(stderr,"Error: Getting Brightness");
	    return -1;
    }
    return c.value;
}
CameraV4L2::ERR CameraV4L2::SetBrightness(int ms)
{
    struct v4l2_control c;
    c.id = 9963776;	// brightness
	c.value = ms;
    if (-1 == xioctl(VIDIOC_S_CTRL, &c))
    {
        fprintf(stderr,"Error: Setting Brightness");
        return FAIL;
    }
	else
		fprintf(stderr,"Set Brightness %d\n",c.value);
    return OK;
}

CameraV4L2::ERR CameraV4L2::PrintCaps()
{
        if (-1 == xioctl(VIDIOC_QUERYCAP, &m_Caps))
        {
                fprintf(stderr,"Error: Querying Capabilities");
                return FAIL;
        }
        printf( "Driver Caps:\n"
                "  Driver: \"%s\"\n"
                "  Card: \"%s\"\n"
                "  Bus: \"%s\"\n"
                "  Version: %d.%d\n"
                "  Capabilities: %08x\n",
                m_Caps.driver,
                m_Caps.card,
                m_Caps.bus_info,
                (m_Caps.version>>16)&&0xff,
                (m_Caps.version>>24)&&0xff,
                m_Caps.capabilities);
  
        struct v4l2_cropcap cropcap = {0};
        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl (VIDIOC_CROPCAP, &cropcap))
        {
                fprintf(stderr,"Error: Querying Cropping Capabilities");
                return FAIL;
        }
 
        printf( "Camera Cropping:\n"
                "  Bounds: %dx%d+%d+%d\n"
                "  Default: %dx%d+%d+%d\n"
                "  Aspect: %d/%d\n",
                cropcap.bounds.width, cropcap.bounds.height, cropcap.bounds.left, cropcap.bounds.top,
                cropcap.defrect.width, cropcap.defrect.height, cropcap.defrect.left, cropcap.defrect.top,
                cropcap.pixelaspect.numerator, cropcap.pixelaspect.denominator);
 
        struct v4l2_fmtdesc fmtdesc = {0};
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        char fourcc[5] = {0};
        char c, e;
        printf("  FMT : CE Desc\n--------------------\n");
        while (0 == xioctl(VIDIOC_ENUM_FMT, &fmtdesc))
        {
                strncpy(fourcc, (char *)&fmtdesc.pixelformat, 4);
                c = fmtdesc.flags & 1? 'C' : ' ';
                e = fmtdesc.flags & 2? 'E' : ' ';
                printf("  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
                fmtdesc.index++;
        }
	
        printf( "Default Camera Mode:\n"
                "  Width: %d\n"
                "  Height: %d\n"
                "  PixFmt: %s\n"
                "  Field: %d\n",
                m_Fmt.fmt.pix.width,
                m_Fmt.fmt.pix.height,
                fourcc,
                m_Fmt.fmt.pix.field);
	
        m_Fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        m_Fmt.fmt.pix.width = m_Width;
        m_Fmt.fmt.pix.height = m_Height;
        m_Fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_Y16;
        m_Fmt.fmt.pix.field = V4L2_FIELD_NONE;
		SetFormat(m_Fmt);
 		GetFormat(m_Fmt);

        strncpy(fourcc, (char *)&m_Fmt.fmt.pix.pixelformat, 4);
        printf( "Setup Camera Mode:\n"
                "  Width: %d\n"
                "  Height: %d\n"
                "  PixFmt: %s\n"
                "  Field: %d\n",
                m_Fmt.fmt.pix.width,
                m_Fmt.fmt.pix.height,
                fourcc,
                m_Fmt.fmt.pix.field);
		
        return OK;
} 
CameraV4L2::ERR CameraV4L2::RequestBuffers(int n)
{
    struct v4l2_requestbuffers req = {0};
    req.count = n;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
 
    if (-1 == xioctl(VIDIOC_REQBUFS, &req))
    {
        fprintf(stderr,"Error: Requesting Buffer");
        return FAIL;
    }
    return OK;
}
int CameraV4L2::WaitForFrame()
{
	m_buf.index = 0;
	QueryBuffer(m_buf);
	if (EnQueBuffer(m_buf))
		return -1;
	
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(m_fd, &fds);
	struct timeval tv = {0};
	tv.tv_sec = 2;
	int r = select(m_fd + 1, &fds, NULL, NULL, &tv);
	if (-1 == r)
	{
		fprintf(stderr, "Error: Waiting for Frame");
		return -1;
	}
	
	if (DeQueBuffer(m_buf))
		return -1;
	return m_buf.bytesused;
}
CameraV4L2::ERR CameraV4L2::Start()
{
 	if(QueryBuffer(m_buf))
		return FAIL;
    if(-1 == xioctl(VIDIOC_STREAMON, &m_buf.type))
    {
        fprintf(stderr,"Error: Start Capture");
        return FAIL;
    }
}
CameraV4L2::ERR CameraV4L2::Stop()
{
 	if(QueryBuffer(m_buf))
		return FAIL;
    if(-1 == xioctl(VIDIOC_STREAMOFF, &m_buf.type))
    {
        fprintf(stderr,"Error: Stop Capture");
        return FAIL;
    }
}
CameraV4L2::ERR CameraV4L2::ConvertTosRGB(cv::Mat &src, cv::Mat &dst)
{
	dst.create(src.size(), src.type());
	unsigned short *pSrcs,*pDsts;
	unsigned char  *pSrcc,*pDstc;
	int width  = src.cols;
	int height = src.rows;
	int i,j;

	switch (src.type())
	{
	case CV_8UC3:
		for (i = 0; i < height; i++)
		{
			pSrcc = (unsigned char *)src.ptr(i);
			pDstc = (unsigned char *)dst.ptr(i);
			for(j=0; j<width*3; j++)
				pDstc[j] = sRGBVal8[pSrcc[j]];
		}
		break;
	case CV_16UC3:
		for (i = 0; i < height; i++)
		{
			pSrcs = (unsigned short *)src.ptr(i);
			pDsts = (unsigned short *)dst.ptr(i);
			for (j = 0; j < width * 3; j++)
			{
				pDsts[j] = sRGBVal16[pSrcs[j]];
			}
		}
		break;
	case CV_8UC1:
		for (i = 0; i < height; i++)
		{
			pSrcc = (unsigned char *)src.ptr(i);
			pDstc = (unsigned char *)dst.ptr(i);
			for(j=0; j<width; j++)
				pDstc[j] = sRGBVal8[pSrcc[j]];
		}
		break;
	case CV_16UC1:
		for (i = 0; i < height; i++)
		{
			pSrcs = (unsigned short *)src.ptr(i);
			pDsts = (unsigned short *)dst.ptr(i);
			for (j = 0; j < width; j++)
			{
				pDsts[j] = sRGBVal16[pSrcs[j]];
			}
		}
		break;
	default:
		break;
	}
}

// ************************************************************************
// ***************  Protected Methods for CameraV4L2  *********************
// ************************************************************************
int CameraV4L2::xioctl(int request, void *arg)
{
        int r;
        do r = ioctl (m_fd, request, arg);
        while (-1 == r && EINTR == errno);
        return r;
}
void CameraV4L2::showflags(int flags)
{
	if(flags & 0x01)
		printf("V4L2_BUF_FLAG_MAPPED\n");
	if(flags & 0x02)
		printf("V4L2_BUF_FLAG_QUEUED\n");
	if(flags & 0x04)
		printf("V4L2_BUF_FLAG_DONE\n");
	if(flags & 0x08)
		printf("V4L2_BUF_FLAG_KEYFRAME\n");
	if(flags & 0x10)
		printf("V4L2_BUF_FLAG_PFRAME\n");
	if(flags & 0x20)
		printf("V4L2_BUF_FLAG_BFRAME\n");
	if(flags & 0x40)
		printf("V4L2_BUF_FLAG_ERROR\n");
	if(flags & 0x100)
		printf("V4L2_BUF_FLAG_TIMECODE\n");
	if(flags & 0x400)
		printf("V4L2_BUF_FLAG_PRERPARED\n");
	if(flags & 0x800)
		printf("V4L2_BUF_FLAG_NO_CACHE_INVALIDATE\n");
	if(flags & 0x1000)
		printf("V4L2_BUF_FLAG_NO_CACHE_CLEAN\n");
	if(flags & 0x4000)
		printf("V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC\n");
	if(flags & 0x8000)
		printf("V4L2_BUF_FLAG_TIMESTAMP_COPY\n");
}
CameraV4L2::ERR CameraV4L2::QueryBuffer(struct v4l2_buffer &buf)
{
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(VIDIOC_QUERYBUF, &buf))
    {
        fprintf(stderr,"Error: Querying Buffer");
        return FAIL;
    }
	return OK;
 }
CameraV4L2::ERR CameraV4L2::EnQueBuffer(struct v4l2_buffer &buf)
{
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(VIDIOC_QBUF, &buf))
    {
        fprintf(stderr,"Error: Enqueue Buffer");
        return FAIL;
    }
    return OK;
}
CameraV4L2::ERR CameraV4L2::DeQueBuffer(struct v4l2_buffer &buf)
{
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(VIDIOC_DQBUF, &buf))
    {
        fprintf(stderr,"Error: DeQue Buffer");
        return FAIL;
    }
	// then Memory Map it to m_Buffer
    m_Buffer = (uint8_t*)mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset);
//  printf("Length: %d\tAddress: %p\tImage Length: %d\n", buf.length, m_Buffer,buf.bytesused);
//	showflags(buf.flags);
 
    return OK;
}

