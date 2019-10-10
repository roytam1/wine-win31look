/*
 * Copyright 1999 Marcus Meissner
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "windef.h"
#include "windows.h"
#include "mmsystem.h"
#include "vfw.h"


int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE prev, LPSTR cmdline, int show)
{
    int			n;
    HRESULT		hres;
    PAVIFILE		avif;
    PAVISTREAM		vids,auds;
    AVIFILEINFO		afi;
    AVISTREAMINFO	asi;

    AVIFileInit();
    if (GetFileAttributes(cmdline) == INVALID_FILE_ATTRIBUTES) {
    	fprintf(stderr,"Usage: aviinfo <avifilename>\n");
	exit(1);
    }
    hres = AVIFileOpen(&avif,cmdline,OF_READ,NULL);
    if (hres) {
    	fprintf(stderr,"AVIFileOpen: 0x%08lx\n",hres);
	exit(1);
    }
    hres = AVIFileInfo(avif,&afi,sizeof(afi));
    if (hres) {
    	fprintf(stderr,"AVIFileInfo: 0x%08lx\n",hres);
	exit(1);
    }
    fprintf(stderr,"AVI File Info:\n");
    fprintf(stderr,"\tdwMaxBytesPerSec: %ld\n",afi.dwMaxBytesPerSec);
#define FF(x) if (afi.dwFlags & AVIFILEINFO_##x) fprintf(stderr,#x",");
    fprintf(stderr,"\tdwFlags: 0x%lx (",afi.dwFlags);
    FF(HASINDEX);FF(MUSTUSEINDEX);FF(ISINTERLEAVED);FF(WASCAPTUREFILE);
    FF(COPYRIGHTED);
    fprintf(stderr,")\n");
#undef FF
#define FF(x) if (afi.dwCaps & AVIFILECAPS_##x) fprintf(stderr,#x",");
    fprintf(stderr,"\tdwCaps: 0x%lx (",afi.dwCaps);
    FF(CANREAD);FF(CANWRITE);FF(ALLKEYFRAMES);FF(NOCOMPRESSION);
    fprintf(stderr,")\n");
#undef FF
    fprintf(stderr,"\tdwStreams: %ld\n",afi.dwStreams);
    fprintf(stderr,"\tdwSuggestedBufferSize: %ld\n",afi.dwSuggestedBufferSize);
    fprintf(stderr,"\tdwWidth: %ld\n",afi.dwWidth);
    fprintf(stderr,"\tdwHeight: %ld\n",afi.dwHeight);
    fprintf(stderr,"\tdwScale: %ld\n",afi.dwScale);
    fprintf(stderr,"\tdwRate: %ld\n",afi.dwRate);
    fprintf(stderr,"\tdwLength: %ld\n",afi.dwLength);
    fprintf(stderr,"\tdwEditCount: %ld\n",afi.dwEditCount);
    fprintf(stderr,"\tszFileType: %s\n",afi.szFileType);

    for (n = 0;n<afi.dwStreams;n++) {
    	    char buf[5];
	    PAVISTREAM	ast;

	    hres = AVIFileGetStream(avif,&ast,0,n);
	    if (hres) {
		fprintf(stderr,"AVIFileGetStream %d: 0x%08lx\n",n,hres);
		exit(1);
	    }
	    hres = AVIStreamInfo(ast,&asi,sizeof(asi));
	    if (hres) {
		fprintf(stderr,"AVIStreamInfo %d: 0x%08lx\n",n,hres);
		exit(1);
	    }
	    fprintf(stderr,"Stream %d:\n",n);
	    buf[4]='\0';memcpy(buf,&(asi.fccType),4);
	    fprintf(stderr,"\tfccType: %s\n",buf);
	    memcpy(buf,&(asi.fccHandler),4);
	    fprintf(stderr,"\tfccHandler: %s\n",buf);
	    fprintf(stderr,"\tdwFlags: 0x%08lx\n",asi.dwFlags);
	    fprintf(stderr,"\tdwCaps: 0x%08lx\n",asi.dwCaps);
	    fprintf(stderr,"\twPriority: %d\n",asi.wPriority);
	    fprintf(stderr,"\twLanguage: %d\n",asi.wLanguage);
	    fprintf(stderr,"\tdwScale: %ld\n",asi.dwScale);
	    fprintf(stderr,"\tdwRate: %ld\n",asi.dwRate);
	    fprintf(stderr,"\tdwStart: %ld\n",asi.dwStart);
	    fprintf(stderr,"\tdwLength: %ld\n",asi.dwLength);
	    fprintf(stderr,"\tdwInitialFrames: %ld\n",asi.dwInitialFrames);
	    fprintf(stderr,"\tdwSuggestedBufferSize: %ld\n",asi.dwSuggestedBufferSize);
	    fprintf(stderr,"\tdwQuality: %ld\n",asi.dwQuality);
	    fprintf(stderr,"\tdwSampleSize: %ld\n",asi.dwSampleSize);
	    fprintf(stderr,"\tdwEditCount: %ld\n",asi.dwEditCount);
	    fprintf(stderr,"\tdwFormatChangeCount: %ld\n",asi.dwFormatChangeCount);
	    fprintf(stderr,"\tszName: %s\n",asi.szName);
	    switch (asi.fccType) {
	    case streamtypeVIDEO:
	    	vids = ast;
		break;
	    case streamtypeAUDIO:
	    	auds = ast;
		break;
	    default:  {
	    	char type[5];
		type[4]='\0';memcpy(type,&(asi.fccType),4);

	    	fprintf(stderr,"Unhandled streamtype %s\n",type);
		break;
	    }
	    }
	    AVIStreamRelease(ast);
    }
    AVIFileRelease(avif);
    AVIFileExit();
    return 0;
}
