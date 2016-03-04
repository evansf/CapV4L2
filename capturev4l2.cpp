#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

// global device
int fd;
struct v4l2_capability caps = {};

// global pointer to current buffer
uint8_t *buffer;

// forward Declarations
static void showflags(int flags);



static int xioctl(int fd, int request, void *arg)
{
        int r;
 
        do r = ioctl (fd, request, arg);
        while (-1 == r && EINTR == errno);
 
        return r;
}
int set_format(struct v4l2_format &fmt)
{      
        if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
        {
            perror("Setting Pixel Format");
            return 1;
        }
}

int print_caps()
{
        if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &caps))
        {
                perror("Querying Capabilities");
                return 1;
        }
 
        printf( "Driver Caps:\n"
                "  Driver: \"%s\"\n"
                "  Card: \"%s\"\n"
                "  Bus: \"%s\"\n"
                "  Version: %d.%d\n"
                "  Capabilities: %08x\n",
                caps.driver,
                caps.card,
                caps.bus_info,
                (caps.version>>16)&&0xff,
                (caps.version>>24)&&0xff,
                caps.capabilities);
 
 
        struct v4l2_cropcap cropcap = {0};
        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl (fd, VIDIOC_CROPCAP, &cropcap))
        {
                perror("Querying Cropping Capabilities");
                return 1;
        }
 
        printf( "Camera Cropping:\n"
                "  Bounds: %dx%d+%d+%d\n"
                "  Default: %dx%d+%d+%d\n"
                "  Aspect: %d/%d\n",
                cropcap.bounds.width, cropcap.bounds.height, cropcap.bounds.left, cropcap.bounds.top,
                cropcap.defrect.width, cropcap.defrect.height, cropcap.defrect.left, cropcap.defrect.top,
                cropcap.pixelaspect.numerator, cropcap.pixelaspect.denominator);
 
        int support_grbg10 = 0;
 
        struct v4l2_fmtdesc fmtdesc = {0};
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        char fourcc[5] = {0};
        char c, e;
        printf("  FMT : CE Desc\n--------------------\n");
        while (0 == xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc))
        {
                strncpy(fourcc, (char *)&fmtdesc.pixelformat, 4);
                if (fmtdesc.pixelformat == V4L2_PIX_FMT_SGRBG10)
                    support_grbg10 = 1;
                c = fmtdesc.flags & 1? 'C' : ' ';
                e = fmtdesc.flags & 2? 'E' : ' ';
                printf("  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
                fmtdesc.index++;
        }
        /*
        if (!support_grbg10)
        {
            printf("Doesn't support GRBG10.\n");
            return 1;
        }*/
	
        struct v4l2_format fmt = {0};
#if (1)
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = 672;
        fmt.fmt.pix.height = 380;
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_Y16;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
 
		set_format(fmt);
#endif	
        strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
        printf( "Selected Camera Mode:\n"
                "  Width: %d\n"
                "  Height: %d\n"
                "  PixFmt: %s\n"
                "  Field: %d\n",
                fmt.fmt.pix.width,
                fmt.fmt.pix.height,
                fourcc,
                fmt.fmt.pix.field);
        return 0;
}
 
int request_buffers(int n)
{
    struct v4l2_requestbuffers req = {0};
    req.count = n;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
 
    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
    {
        perror("Requesting Buffer");
        return 1;
    }
}
int query_buffer(struct v4l2_buffer &buf)
{
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
    {
        perror("Querying Buffer");
        return 1;
    }
 }
int enque_buffer(struct v4l2_buffer &buf)
{
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(fd, VIDIOC_QBUF, &buf))
    {
        perror("Retrieving Buffer");
        return 1;
    }
    return 0;
}

int get_buffer(struct v4l2_buffer &buf)
{
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
    {
        perror("Retrieving Buffer");
        return 1;
    }
    buffer = (uint8_t*)mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
	
    printf("Length: %d\tAddress: %p\tImage Length: %d\n", buf.length, buffer,buf.bytesused);
	showflags(buf.flags);
 
    return 0;
}

//Macros to assist in Bayer Conversion
#define R(x,y,w) pDstRGB[0 + 3 * ((x) + (w) * (y))] 
#define G(x,y,w) pDstRGB[1 + 3 * ((x) + (w) * (y))] 
#define B(x,y,w) pDstRGB[2 + 3 * ((x) + (w) * (y))] 
#define IR(x,y,w) pDstIR[0 +     ((x) + (w) * (y))] 
#define BAY(x,y,w) pSrc[(x) + (w) * (y)]
#define CLIP(x) ((x) < 0 ? 0 : ((x) >= 255 ? 255 : (x)))

// to call this routine, do...  dstRGB create(width, height, CV_8UC3)
//                         and   dstIR create(width, height, CV_8UC1)
cv::Point2i ExtractBayer10_Y16(cv::Mat &dstRGB,cv::Mat &dstIR, uint8_t * src, int srcLen, cv::Point2i start = cv::Point2i(0,0))
{
	cv::Point2i last;
	unsigned char *pDstRGB;
	unsigned char *pDstIR;
	unsigned short *pSrc = (unsigned short*) src;
	int x,y;
	int width = dstRGB.cols;  int height = dstRGB.rows;
	// itterate 2X2 to de-Bayer and spread out R,G,B elements ot RGB and put IR to IR
	for (y = start.y; (y < height) ; y+=2)
	{
		if( (x + width * y)*2 >= srcLen )
			break;
		pDstRGB = dstRGB.ptr(y);
		pDstIR  = dstIR.ptr(y);
		for (x = start.x; (x < width) ; x+=2)
		{
			B(x,0,width)   = B(x+1,0, width) =  B(x,1,width) =  B(x+1,1,width) = CLIP(BAY(x  ,y  ,width));
			G(x,0,width)   = G(x+1,0, width) =  G(x,1,width) =  G(x+1,1,width) = CLIP(BAY(x+1,y  ,width));
			R(x,0,width)   = R(x+1,0, width) =  R(x,1,width) =  R(x+1,1,width) = CLIP(BAY(x+1,y+1,width));
			IR(x,0,width) = IR(x+1,0, width) = IR(x,1,width) = IR(x+1,1,width) = CLIP(BAY(x  ,y+1,width));
		}
	}
	last.x = x; last.y = y;
	return last;
}

int capture_image()
{
    struct v4l2_buffer buf = {0};
 	if(query_buffer(buf))
		return 1;
    if(-1 == xioctl(fd, VIDIOC_STREAMON, &buf.type))
    {
        perror("Start Capture");
        return 1;
    }
	cv::Mat frameRGB;
	cv::Mat frameIR;
	int width=672, height=380;
	frameRGB.create(height,width,CV_8UC3);
	 frameIR.create(height,width,CV_8UC1);
	cv::Point2i start = cv::Point2i(0,0);
//	bool PartialImage = true;
	do
	{
		if (enque_buffer(buf))
			return 1;

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		struct timeval tv = {0};
		tv.tv_sec = 2;
		int r = select(fd + 1, &fds, NULL, NULL, &tv);
		if (-1 == r)
		{
			perror("Waiting for Frame");
			return 1;
		}
	
		if (get_buffer(buf))
			return 1;
		int bufLen = buf.bytesused;
		start = ExtractBayer10_Y16(frameRGB,frameIR,buffer,bufLen,start);
	} while (start.y < height);
	
    printf ("saving image\n");
	
	cv::imshow("frameRGB",frameRGB);
	cv::imshow("frameIR",frameIR);
	cv::waitKey();
	cv::imwrite("/home/frank/imageRGB.jpg",frameRGB);
	cv::imwrite("/home/frank/imageIR.jpg",frameIR);
	
    return 0;
}
 
static void showflags(int flags)
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

int main()
{
 
        fd = open("/dev/video0", O_RDWR);
        if (fd == -1)
        {
                perror("Opening video device");
                return 1;
        }
        if(print_caps())
            return 1;
		if(request_buffers(1))
			return 1;

        if(capture_image())
            return 1;
	
        close(fd);
        return 0;
}