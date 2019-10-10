/*
 * Nt time functions.
 *
 * RtlTimeToTimeFields, RtlTimeFieldsToTime and defines are taken from ReactOS and
 * adapted to wine with special permissions of the author. This code is
 * Copyright 2002 Rex Jolliff (rex@lvcablemodem.com)
 *
 * Copyright 1999 Juergen Schmied
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

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winternl.h"
#include "wine/unicode.h"
#include "wine/debug.h"
#include "ntdll_misc.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);

static CRITICAL_SECTION TIME_GetBias_section;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &TIME_GetBias_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { 0, (DWORD)(__FILE__ ": TIME_GetBias_section") }
};
static CRITICAL_SECTION TIME_GetBias_section = { &critsect_debug, -1, 0, 0, 0, 0 };


#define SETTIME_MAX_ADJUST 120

/* This structure is used to store strings that represent all of the time zones
 * in the world. (This is used to help GetTimeZoneInformation)
 */
struct tagTZ_INFO
{
    const char *psTZFromUnix;
    WCHAR psTZWindows[32];
    int bias;
    int dst;
};

static const struct tagTZ_INFO TZ_INFO[] =
{
   {"MHT",
    {'D','a','t','e','l','i','n','e',' ','S','t','a','n','d','a','r','d',' ',
     'T','i','m','e','\0'}, -720, 0},
   {"SST",
    {'S','a','m','o','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m',
     'e','\0'}, 660, 0},
   {"HST",
    {'H','a','w','a','i','i','a','n',' ','S','t','a','n','d','a','r','d',' ',
     'T','i','m','e','\0'}, 600, 0},
   {"AKDT",
    {'A','l','a','s','k','a','n',' ','S','t','a','n','d','a','r','d',' ','T',
     'i','m','e','\0'}, 480, 1},
   {"PDT",
    {'P','a','c','i','f','i','c',' ','S','t','a','n','d','a','r','d',' ','T',
     'i','m','e','\0'}, 420, 1},
   {"MST",
    {'U','S',' ','M','o','u','n','t','a','i','n',' ','S','t','a','n','d','a',
     'r','d',' ','T','i','m','e','\0'}, 420, 0},
   {"MDT",
    {'M','o','u','n','t','a','i','n',' ','S','t','a','n','d','a','r','d',' ',
     'T','i','m','e','\0'}, 360, 1},
   {"CST",
    {'C','e','n','t','r','a','l',' ','A','m','e','r','i','c','a',' ','S','t',
     'a','n','d','a','r','d',' ','T','i','m','e','\0'}, 360, 0},
   {"CDT",
    {'C','e','n','t','r','a','l',' ','S','t','a','n','d','a','r','d',' ','T',
     'i','m','e','\0'}, 300, 1},
   {"COT",
    {'S','A',' ','P','a','c','i','f','i','c',' ','S','t','a','n','d','a','r',
     'd',' ','T','i','m','e','\0'}, 300, 0},
   {"EDT",
    {'E','a','s','t','e','r','n',' ','S','t','a','n','d','a','r','d',' ','T',
     'i','m','e','\0'}, 240, 1},
   {"EST",
    {'U','S',' ','E','a','s','t','e','r','n',' ','S','t','a','n','d','a','r',
     'd',' ','T','i','m','e','\0'}, 300, 0},
   {"ADT",
    {'A','t','l','a','n','t','i','c',' ','S','t','a','n','d','a','r','d',' ',
     'T','i','m','e','\0'}, 180, 1},
   {"VET",
    {'S','A',' ','W','e','s','t','e','r','n',' ','S','t','a','n','d','a','r',
     'd',' ','T','i','m','e','\0'}, 240, 0},
   {"CLT",
    {'P','a','c','i','f','i','c',' ','S','A',' ','S','t','a','n','d','a','r',
     'd',' ','T','i','m','e','\0'}, 240, 0},
   {"NDT",
    {'N','e','w','f','o','u','n','d','l','a','n','d',' ','S','t','a','n','d',
     'a','r','d',' ','T','i','m','e','\0'}, 150, 1},
   {"BRT",
    {'E','.',' ','S','o','u','t','h',' ','A','m','e','r','i','c','a',' ','S',
     't','a','n','d','a','r','d',' ','T','i','m','e','\0'}, 180, 0},
   {"ART",
    {'S','A',' ','E','a','s','t','e','r','n',' ','S','t','a','n','d','a','r',
     'd',' ','T','i','m','e','\0'}, 180, 0},
   {"WGST",
    {'G','r','e','e','n','l','a','n','d',' ','S','t','a','n','d','a','r','d',
     ' ','T','i','m','e','\0'}, 120, 1},
   {"GST",
    {'M','i','d','-','A','t','l','a','n','t','i','c',' ','S','t','a','n','d',
     'a','r','d',' ','T','i','m','e','\0'}, 120, 0},
   {"AZOST",
    {'A','z','o','r','e','s',' ','S','t','a','n','d','a','r','d',' ','T','i',
     'm','e','\0'}, 0, 1},
   {"CVT",
    {'C','a','p','e',' ','V','e','r','d','e',' ','S','t','a','n','d','a','r',
     'd',' ','T','i','m','e','\0'}, 60, 0},
   {"WET",
    {'G','r','e','e','n','w','i','c','h',' ','S','t','a','n','d','a','r','d',
     ' ','T','i','m','e','\0'}, 0, 0},
   {"BST",
    {'G','M','T',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -60, 1},
   {"GMT",
    {'G','M','T',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    0, 0},
   {"CEST",
    {'C','e','n','t','r','a','l',' ','E','u','r','o','p','e',' ','S','t','a',
     'n','d','a','r','d',' ','T','i','m','e','\0'}, -120, 1},
   {"WAT",
    {'W','.',' ','C','e','n','t','r','a','l',' ','A','f','r','i','c','a',' ',
     'S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'}, -60, 0},
   {"EEST",
    {'E','.',' ','E','u','r','o','p','e',' ','S','t','a','n','d','a','r','d',
     ' ','T','i','m','e','\0'}, -180, 1},
   {"EET",
    {'E','g','y','p','t',' ','S','t','a','n','d','a','r','d',' ','T','i','m',
     'e','\0'}, -120, 0},
   {"CAT",
    {'S','o','u','t','h',' ','A','f','r','i','c','a',' ','S','t','a','n','d',
     'a','r','d',' ','T','i','m','e','\0'}, -120, 0},
   {"IST",
    {'I','s','r','a','e','l',' ','S','t','a','n','d','a','r','d',' ','T','i',
     'm','e','\0'}, -120, 0},
   {"ADT",
    {'A','r','a','b','i','c',' ','S','t','a','n','d','a','r','d',' ','T','i',
     'm','e','\0'}, -240, 1},
   {"AST",
    {'A','r','a','b',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e',
     '\0'}, -180, 0},
   {"MSD",
    {'R','u','s','s','i','a','n',' ','S','t','a','n','d','a','r','d',' ','T',
     'i','m','e','\0'}, -240, 1},
   {"EAT",
    {'E','.',' ','A','f','r','i','c','a',' ','S','t','a','n','d','a','r','d',
     ' ','T','i','m','e','\0'}, -180, 0},
   {"IRST",
    {'I','r','a','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e',
     '\0'}, -270, 1},
   {"GST",
    {'A','r','a','b','i','a','n',' ','S','t','a','n','d','a','r','d',' ','T',
     'i','m','e','\0'}, -240, 0},
   {"AZST",
    {'C','a','u','c','a','s','u','s',' ','S','t','a','n','d','a','r','d',' ',
     'T','i','m','e','\0'}, -300, 1},
   {"AFT",
    {'A','f','g','h','a','n','i','s','t','a','n',' ','S','t','a','n','d','a',
     'r','d',' ','T','i','m','e','\0'}, -270, 0},
   {"YEKST",
    {'E','k','a','t','e','r','i','n','b','u','r','g',' ','S','t','a','n','d',
     'a','r','d',' ','T','i','m','e','\0'}, -360, 1},
   {"PKT",
    {'W','e','s','t',' ','A','s','i','a',' ','S','t','a','n','d','a','r','d',
     ' ','T','i','m','e','\0'}, -300, 0},
   {"IST",
    {'I','n','d','i','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m',
     'e','\0'}, -330, 0},
   {"NPT",
    {'N','e','p','a','l',' ','S','t','a','n','d','a','r','d',' ','T','i','m',
     'e','\0'}, -345, 0},
   {"ALMST",
    {'N','.',' ','C','e','n','t','r','a','l',' ','A','s','i','a',' ','S','t',
     'a','n','d','a','r','d',' ','T','i','m','e','\0'}, -420, 1},
   {"BDT",
    {'C','e','n','t','r','a','l',' ','A','s','i','a',' ','S','t','a','n','d',
     'a','r','d',' ','T','i','m','e','\0'}, -360, 0},
   {"LKT",
    {'S','r','i',' ','L','a','n','k','a',' ','S','t','a','n','d','a','r','d',
     ' ','T','i','m','e','\0'}, -360, 0},
   {"MMT",
    {'M','y','a','n','m','a','r',' ','S','t','a','n','d','a','r','d',' ','T',
     'i','m','e','\0'}, -390, 0},
   {"ICT",
    {'S','E',' ','A','s','i','a',' ','S','t','a','n','d','a','r','d',' ','T',
     'i','m','e','\0'}, -420, 0},
   {"KRAST",
    {'N','o','r','t','h',' ','A','s','i','a',' ','S','t','a','n','d','a','r',
     'd',' ','T','i','m','e','\0'}, -480, 1},
   {"CST",
    {'C','h','i','n','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m',
     'e','\0'}, -480, 0},
   {"IRKST",
    {'N','o','r','t','h',' ','A','s','i','a',' ','E','a','s','t',' ','S','t',
     'a','n','d','a','r','d',' ','T','i','m','e','\0'}, -540, 1},
   {"SGT",
    {'M','a','l','a','y',' ','P','e','n','i','n','s','u','l','a',' ','S','t',
     'a','n','d','a','r','d',' ','T','i','m','e','\0'}, -480, 0},
   {"WST",
    {'W','.',' ','A','u','s','t','r','a','l','i','a',' ','S','t','a','n','d',
     'a','r','d',' ','T','i','m','e','\0'}, -480, 0},
   {"JST",
    {'T','o','k','y','o',' ','S','t','a','n','d','a','r','d',' ','T','i','m',
     'e','\0'}, -540, 0},
   {"KST",
    {'K','o','r','e','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m',
     'e','\0'}, -540, 0},
   {"YAKST",
    {'Y','a','k','u','t','s','k',' ','S','t','a','n','d','a','r','d',' ','T',
     'i','m','e','\0'}, -600, 1},
   {"CST",
    {'C','e','n','.',' ','A','u','s','t','r','a','l','i','a',' ','S','t','a',
     'n','d','a','r','d',' ','T','i','m','e','\0'}, -570, 0},
   {"EST",
    {'E','.',' ','A','u','s','t','r','a','l','i','a',' ','S','t','a','n','d',
     'a','r','d',' ','T','i','m','e','\0'}, -600, 0},
   {"GST",
    {'W','e','s','t',' ','P','a','c','i','f','i','c',' ','S','t','a','n','d',
     'a','r','d',' ','T','i','m','e','\0'}, -600, 0},
   {"VLAST",
    {'V','l','a','d','i','v','o','s','t','o','k',' ','S','t','a','n','d','a',
     'r','d',' ','T','i','m','e','\0'}, -660, 1},
   {"MAGST",
    {'C','e','n','t','r','a','l',' ','P','a','c','i','f','i','c',' ','S','t',
     'a','n','d','a','r','d',' ','T','i','m','e','\0'}, -720, 1},
   {"NZST",
    {'N','e','w',' ','Z','e','a','l','a','n','d',' ','S','t','a','n','d','a',
     'r','d',' ','T','i','m','e','\0'}, -720, 0},
   {"FJT",
    {'F','i','j','i',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e',
     '\0'}, -720, 0},
   {"TOT",
    {'T','o','n','g','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m',
     'e','\0'}, -780, 0}
};

#define TICKSPERSEC        10000000
#define TICKSPERMSEC       10000
#define SECSPERDAY         86400
#define SECSPERHOUR        3600
#define SECSPERMIN         60
#define MINSPERHOUR        60
#define HOURSPERDAY        24
#define EPOCHWEEKDAY       1  /* Jan 1, 1601 was Monday */
#define DAYSPERWEEK        7
#define EPOCHYEAR          1601
#define DAYSPERNORMALYEAR  365
#define DAYSPERLEAPYEAR    366
#define MONSPERYEAR        12
#define DAYSPERQUADRICENTENNIUM (365 * 400 + 97)
#define DAYSPERNORMALCENTURY (365 * 100 + 24)
#define DAYSPERNORMALQUADRENNIUM (365 * 4 + 1)

/* 1601 to 1970 is 369 years plus 89 leap days */
#define SECS_1601_TO_1970  ((369 * 365 + 89) * (ULONGLONG)SECSPERDAY)
#define TICKS_1601_TO_1970 (SECS_1601_TO_1970 * TICKSPERSEC)
/* 1601 to 1980 is 379 years plus 91 leap days */
#define SECS_1601_TO_1980  ((379 * 365 + 91) * (ULONGLONG)SECSPERDAY)
#define TICKS_1601_TO_1980 (SECS_1601_TO_1980 * TICKSPERSEC)


static const int MonthLengths[2][MONSPERYEAR] =
{
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static inline int IsLeapYear(int Year)
{
	return Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0) ? 1 : 0;
}

static inline void NormalizeTimeFields(CSHORT *FieldToNormalize, CSHORT *CarryField,int Modulus)
{
	*FieldToNormalize = (CSHORT) (*FieldToNormalize - Modulus);
	*CarryField = (CSHORT) (*CarryField + 1);
}

/***********************************************************************
 *              NTDLL_get_server_timeout
 *
 * Convert a NTDLL timeout into a timeval struct to send to the server.
 */
void NTDLL_get_server_timeout( abs_time_t *when, const LARGE_INTEGER *timeout )
{
    UINT remainder;

    if (!timeout)  /* infinite timeout */
    {
        when->sec = when->usec = 0;
    }
    else if (timeout->QuadPart <= 0)  /* relative timeout */
    {
        struct timeval tv;
        ULONG sec = RtlEnlargedUnsignedDivide( -timeout->QuadPart, TICKSPERSEC, &remainder );
        gettimeofday( &tv, 0 );
        when->sec = tv.tv_sec + sec;
        if ((when->usec = tv.tv_usec + (remainder / 10)) >= 1000000)
        {
            when->usec -= 1000000;
            when->sec++;
        }
    }
    else  /* absolute time */
    {
        when->sec = RtlEnlargedUnsignedDivide( timeout->QuadPart - TICKS_1601_TO_1970,
                                                  TICKSPERSEC, &remainder );
        when->usec = remainder / 10;
    }
}


/******************************************************************************
 *       RtlTimeToTimeFields [NTDLL.@]
 *
 * Convert a time into a TIME_FIELDS structure.
 *
 * PARAMS
 *   liTime     [I] Time to convert.
 *   TimeFields [O] Destination for the converted time.
 *
 * RETURNS
 *   Nothing.
 */
VOID WINAPI RtlTimeToTimeFields(
	const LARGE_INTEGER *liTime,
	PTIME_FIELDS TimeFields)
{
	const int *Months;
	int SecondsInDay, DeltaYear;
	int LeapYear, CurMonth;
	long int Days;
	LONGLONG Time = liTime->QuadPart;

	/* Extract millisecond from time and convert time into seconds */
	TimeFields->Milliseconds = (CSHORT) ((Time % TICKSPERSEC) / TICKSPERMSEC);
	Time = Time / TICKSPERSEC;

	/* The native version of RtlTimeToTimeFields does not take leap seconds
	 * into account */

	/* Split the time into days and seconds within the day */
	Days = Time / SECSPERDAY;
	SecondsInDay = Time % SECSPERDAY;

	/* compute time of day */
	TimeFields->Hour = (CSHORT) (SecondsInDay / SECSPERHOUR);
	SecondsInDay = SecondsInDay % SECSPERHOUR;
	TimeFields->Minute = (CSHORT) (SecondsInDay / SECSPERMIN);
	TimeFields->Second = (CSHORT) (SecondsInDay % SECSPERMIN);

	/* compute day of week */
	TimeFields->Weekday = (CSHORT) ((EPOCHWEEKDAY + Days) % DAYSPERWEEK);

	/* compute year */
	/* FIXME: handle calendar modifications */
        TimeFields->Year = EPOCHYEAR;
        DeltaYear = Days / DAYSPERQUADRICENTENNIUM;
        TimeFields->Year += DeltaYear * 400;
        Days -= DeltaYear * DAYSPERQUADRICENTENNIUM;
        DeltaYear = Days / DAYSPERNORMALCENTURY;
        TimeFields->Year += DeltaYear * 100;
        Days -= DeltaYear * DAYSPERNORMALCENTURY;
        DeltaYear = Days / DAYSPERNORMALQUADRENNIUM;
        TimeFields->Year += DeltaYear * 4;
        Days -= DeltaYear * DAYSPERNORMALQUADRENNIUM;
        DeltaYear = Days / DAYSPERNORMALYEAR;
        TimeFields->Year += DeltaYear;
        Days -= DeltaYear * DAYSPERNORMALYEAR;

        LeapYear = IsLeapYear(TimeFields->Year);

	/* Compute month of year */
	Months = MonthLengths[LeapYear];
	for (CurMonth = 0; Days >= (long) Months[CurMonth]; CurMonth++)
	  Days = Days - (long) Months[CurMonth];
	TimeFields->Month = (CSHORT) (CurMonth + 1);
	TimeFields->Day = (CSHORT) (Days + 1);
}

/******************************************************************************
 *       RtlTimeFieldsToTime [NTDLL.@]
 *
 * Convert a TIME_FIELDS structure into a time.
 *
 * PARAMS
 *   ftTimeFields [I] TIME_FIELDS structure to convert.
 *   Time         [O] Destination for the converted time.
 *
 * RETURNS
 *   Success: TRUE.
 *   Failure: FALSE.
 */
BOOLEAN WINAPI RtlTimeFieldsToTime(
	PTIME_FIELDS tfTimeFields,
	PLARGE_INTEGER Time)
{
	int CurYear, CurMonth, DeltaYear;
	LONGLONG rcTime;
	TIME_FIELDS TimeFields = *tfTimeFields;

	rcTime = 0;

	/* FIXME: normalize the TIME_FIELDS structure here */
	while (TimeFields.Second >= SECSPERMIN)
	{ NormalizeTimeFields(&TimeFields.Second, &TimeFields.Minute, SECSPERMIN);
	}
	while (TimeFields.Minute >= MINSPERHOUR)
	{ NormalizeTimeFields(&TimeFields.Minute, &TimeFields.Hour, MINSPERHOUR);
	}
	while (TimeFields.Hour >= HOURSPERDAY)
	{ NormalizeTimeFields(&TimeFields.Hour, &TimeFields.Day, HOURSPERDAY);
	}
	while (TimeFields.Day > MonthLengths[IsLeapYear(TimeFields.Year)][TimeFields.Month - 1])
	{ NormalizeTimeFields(&TimeFields.Day, &TimeFields.Month, SECSPERMIN);
	}
	while (TimeFields.Month > MONSPERYEAR)
	{ NormalizeTimeFields(&TimeFields.Month, &TimeFields.Year, MONSPERYEAR);
	}

	/* FIXME: handle calendar corrections here */
        CurYear = TimeFields.Year - EPOCHYEAR;
        DeltaYear = CurYear / 400;
        CurYear -= DeltaYear * 400;
        rcTime += DeltaYear * DAYSPERQUADRICENTENNIUM;
        DeltaYear = CurYear / 100;
        CurYear -= DeltaYear * 100;
        rcTime += DeltaYear * DAYSPERNORMALCENTURY;
        DeltaYear = CurYear / 4;
        CurYear -= DeltaYear * 4;
        rcTime += DeltaYear * DAYSPERNORMALQUADRENNIUM;
        rcTime += CurYear * DAYSPERNORMALYEAR;

	for (CurMonth = 1; CurMonth < TimeFields.Month; CurMonth++)
	{ rcTime += MonthLengths[IsLeapYear(CurYear)][CurMonth - 1];
	}
	rcTime += TimeFields.Day - 1;
	rcTime *= SECSPERDAY;
	rcTime += TimeFields.Hour * SECSPERHOUR + TimeFields.Minute * SECSPERMIN + TimeFields.Second;
	rcTime *= TICKSPERSEC;
	rcTime += TimeFields.Milliseconds * TICKSPERMSEC;
	Time->QuadPart = rcTime;

	return TRUE;
}

/***********************************************************************
 *       TIME_GetBias [internal]
 *
 * Helper function calculates delta local time from UTC. 
 *
 * PARAMS
 *   utc [I] The current utc time.
 *   pdaylight [I] Local daylight.
 *
 * RETURNS
 *   The bias for the current timezone.
 */
static int TIME_GetBias(time_t utc, int *pdaylight)
{
    struct tm *ptm;
    static time_t last_utc;
    static int last_bias;
    static int last_daylight;
    int ret;

    RtlEnterCriticalSection( &TIME_GetBias_section );
    if(utc == last_utc)
    {
        *pdaylight = last_daylight;
        ret = last_bias;	
    } else
    {
        ptm = localtime(&utc);
	*pdaylight = last_daylight =
            ptm->tm_isdst; /* daylight for local timezone */
	ptm = gmtime(&utc);
	ptm->tm_isdst = *pdaylight; /* use local daylight, not that of Greenwich */
	last_utc = utc;
	ret = last_bias = (int)(utc-mktime(ptm));
    }
    RtlLeaveCriticalSection( &TIME_GetBias_section );
    return ret;
}

/******************************************************************************
 *        RtlLocalTimeToSystemTime [NTDLL.@]
 *
 * Convert a local time into system time.
 *
 * PARAMS
 *   LocalTime  [I] Local time to convert.
 *   SystemTime [O] Destination for the converted time.
 *
 * RETURNS
 *   Success: STATUS_SUCCESS.
 *   Failure: An NTSTATUS error code indicating the problem.
 */
NTSTATUS WINAPI RtlLocalTimeToSystemTime( const LARGE_INTEGER *LocalTime,
                                          PLARGE_INTEGER SystemTime)
{
    time_t gmt;
    int bias, daylight;

    TRACE("(%p, %p)\n", LocalTime, SystemTime);

    gmt = time(NULL);
    bias = TIME_GetBias(gmt, &daylight);

    SystemTime->QuadPart = LocalTime->QuadPart - bias * (LONGLONG)10000000;
    return STATUS_SUCCESS;
}

/******************************************************************************
 *       RtlSystemTimeToLocalTime [NTDLL.@]
 *
 * Convert a system time into a local time.
 *
 * PARAMS
 *   SystemTime [I] System time to convert.
 *   LocalTime  [O] Destination for the converted time.
 *
 * RETURNS
 *   Success: STATUS_SUCCESS.
 *   Failure: An NTSTATUS error code indicating the problem.
 */
NTSTATUS WINAPI RtlSystemTimeToLocalTime( const LARGE_INTEGER *SystemTime,
                                          PLARGE_INTEGER LocalTime )
{
    time_t gmt;
    int bias, daylight;

    TRACE("(%p, %p)\n", SystemTime, LocalTime);

    gmt = time(NULL);
    bias = TIME_GetBias(gmt, &daylight);

    LocalTime->QuadPart = SystemTime->QuadPart + bias * (LONGLONG)10000000;
    return STATUS_SUCCESS;
}

/******************************************************************************
 *       RtlTimeToSecondsSince1970 [NTDLL.@]
 *
 * Convert a time into a count of seconds since 1970.
 *
 * PARAMS
 *   Time    [I] Time to convert.
 *   Seconds [O] Destination for the converted time.
 *
 * RETURNS
 *   Success: TRUE.
 *   Failure: FALSE, if the resulting value will not fit in a DWORD.
 */
BOOLEAN WINAPI RtlTimeToSecondsSince1970( const LARGE_INTEGER *Time, LPDWORD Seconds )
{
    ULONGLONG tmp = ((ULONGLONG)Time->u.HighPart << 32) | Time->u.LowPart;
    tmp = RtlLargeIntegerDivide( tmp, 10000000, NULL );
    tmp -= SECS_1601_TO_1970;
    if (tmp > 0xffffffff) return FALSE;
    *Seconds = (DWORD)tmp;
    return TRUE;
}

/******************************************************************************
 *       RtlTimeToSecondsSince1980 [NTDLL.@]
 *
 * Convert a time into a count of seconds since 1980.
 *
 * PARAMS
 *   Time    [I] Time to convert.
 *   Seconds [O] Destination for the converted time.
 *
 * RETURNS
 *   Success: TRUE.
 *   Failure: FALSE, if the resulting value will not fit in a DWORD.
 */
BOOLEAN WINAPI RtlTimeToSecondsSince1980( const LARGE_INTEGER *Time, LPDWORD Seconds )
{
    ULONGLONG tmp = ((ULONGLONG)Time->u.HighPart << 32) | Time->u.LowPart;
    tmp = RtlLargeIntegerDivide( tmp, 10000000, NULL );
    tmp -= SECS_1601_TO_1980;
    if (tmp > 0xffffffff) return FALSE;
    *Seconds = (DWORD)tmp;
    return TRUE;
}

/******************************************************************************
 *       RtlSecondsSince1970ToTime [NTDLL.@]
 *
 * Convert a count of seconds since 1970 to a time.
 *
 * PARAMS
 *   Seconds [I] Time to convert.
 *   Time    [O] Destination for the converted time.
 *
 * RETURNS
 *   Nothing.
 */
void WINAPI RtlSecondsSince1970ToTime( DWORD Seconds, LARGE_INTEGER *Time )
{
    ULONGLONG secs = Seconds * (ULONGLONG)TICKSPERSEC + TICKS_1601_TO_1970;
    Time->u.LowPart  = (DWORD)secs;
    Time->u.HighPart = (DWORD)(secs >> 32);
}

/******************************************************************************
 *       RtlSecondsSince1980ToTime [NTDLL.@]
 *
 * Convert a count of seconds since 1980 to a time.
 *
 * PARAMS
 *   Seconds [I] Time to convert.
 *   Time    [O] Destination for the converted time.
 *
 * RETURNS
 *   Nothing.
 */
void WINAPI RtlSecondsSince1980ToTime( DWORD Seconds, LARGE_INTEGER *Time )
{
    ULONGLONG secs = Seconds * (ULONGLONG)TICKSPERSEC + TICKS_1601_TO_1980;
    Time->u.LowPart  = (DWORD)secs;
    Time->u.HighPart = (DWORD)(secs >> 32);
}

/******************************************************************************
 *       RtlTimeToElapsedTimeFields [NTDLL.@]
 *
 * Convert a time to a count of elapsed seconds.
 *
 * PARAMS
 *   Time       [I] Time to convert.
 *   TimeFields [O] Destination for the converted time.
 *
 * RETURNS
 *   Nothing.
 */
void WINAPI RtlTimeToElapsedTimeFields( const LARGE_INTEGER *Time, PTIME_FIELDS TimeFields )
{
    LONGLONG time;
    UINT rem;

    time = RtlExtendedLargeIntegerDivide( Time->QuadPart, TICKSPERSEC, &rem );
    TimeFields->Milliseconds = rem / TICKSPERMSEC;

    /* time is now in seconds */
    TimeFields->Year  = 0;
    TimeFields->Month = 0;
    TimeFields->Day   = RtlExtendedLargeIntegerDivide( time, SECSPERDAY, &rem );

    /* rem is now the remaining seconds in the last day */
    TimeFields->Second = rem % 60;
    rem /= 60;
    TimeFields->Minute = rem % 60;
    TimeFields->Hour = rem / 60;
}

/***********************************************************************
 *       NtQuerySystemTime [NTDLL.@]
 *       ZwQuerySystemTime [NTDLL.@]
 *
 * Get the current system time.
 *
 * PARAMS
 *   Time [O] Destination for the current system time.
 *
 * RETURNS
 *   Success: STATUS_SUCCESS.
 *   Failure: An NTSTATUS error code indicating the problem.
 */
NTSTATUS WINAPI NtQuerySystemTime( PLARGE_INTEGER Time )
{
    struct timeval now;

    gettimeofday( &now, 0 );
    Time->QuadPart = now.tv_sec * (ULONGLONG)TICKSPERSEC + TICKS_1601_TO_1970;
    Time->QuadPart += now.tv_usec * 10;
    return STATUS_SUCCESS;
}

/***********************************************************************
 *        TIME_GetTZAsStr [internal]
 *
 * Helper function that returns the given timezone as a string.
 *
 * PARAMS
 *   utc [I] The current utc time.
 *   bias [I] The bias of the current timezone.
 *   dst [I] ??
 *
 * RETURNS
 *   Timezone name.
 *
 * NOTES:
 *   This could be done with a hash table instead of merely iterating through a
 *   table, however with the small amount of entries (60 or so) I didn't think
 *   it was worth it.
 */
static const WCHAR* TIME_GetTZAsStr (time_t utc, int bias, int dst)
{
   char psTZName[7];
   struct tm *ptm = localtime(&utc);
   int i;

   if (!strftime (psTZName, 7, "%Z", ptm))
      return (NULL);

   for (i=0; i<(sizeof(TZ_INFO) / sizeof(struct tagTZ_INFO)); i++)
   {
      if ( strcmp(TZ_INFO[i].psTZFromUnix, psTZName) == 0 &&
           TZ_INFO[i].bias == bias &&
           TZ_INFO[i].dst == dst
         )
            return TZ_INFO[i].psTZWindows;
   }

   return NULL;
}

/***********************************************************************
 *      RtlQueryTimeZoneInformation [NTDLL.@]
 *
 * Get information about the current timezone.
 *
 * PARAMS
 *   tzinfo [O] Destination for the retrieved timezone info.
 *
 * RETURNS
 *   Success: STATUS_SUCCESS.
 *   Failure: An NTSTATUS error code indicating the problem.
 */
NTSTATUS WINAPI RtlQueryTimeZoneInformation(LPTIME_ZONE_INFORMATION tzinfo)
{
    time_t gmt;
    int bias, daylight;
    const WCHAR *psTZ;

    memset(tzinfo, 0, sizeof(TIME_ZONE_INFORMATION));

    gmt = time(NULL);
    bias = TIME_GetBias(gmt, &daylight);

    tzinfo->Bias = -bias / 60;
    tzinfo->StandardBias = 0;
    tzinfo->DaylightBias = -60;
    psTZ = TIME_GetTZAsStr (gmt, (-bias/60), daylight);
    if (psTZ) strcpyW( tzinfo->StandardName, psTZ );
    return STATUS_SUCCESS;
}

/***********************************************************************
 *       RtlSetTimeZoneInformation [NTDLL.@]
 *
 * Set the current time zone information.
 *
 * PARAMS
 *   tzinfo [I] Timezone information to set.
 *
 * RETURNS
 *   Success: STATUS_SUCCESS.
 *   Failure: An NTSTATUS error code indicating the problem.
 *
 * BUGS
 *   Uses the obsolete unix timezone structure and tz_dsttime member.
 */
NTSTATUS WINAPI RtlSetTimeZoneInformation( const TIME_ZONE_INFORMATION *tzinfo )
{
#ifdef HAVE_SETTIMEOFDAY
    struct timezone tz;

    tz.tz_minuteswest = tzinfo->Bias;
#ifdef DST_NONE
    tz.tz_dsttime = DST_NONE;
#else
    tz.tz_dsttime = 0;
#endif
    if(!settimeofday(NULL, &tz))
        return STATUS_SUCCESS;
#endif
    return STATUS_PRIVILEGE_NOT_HELD;
}

/***********************************************************************
 *        NtSetSystemTime [NTDLL.@]
 *        ZwSetSystemTime [NTDLL.@]
 *
 * Set the system time.
 *
 * PARAMS
 *   NewTime [I] The time to set.
 *   OldTime [O] Optional destination for the previous system time.
 *
 * RETURNS
 *   Success: STATUS_SUCCESS.
 *   Failure: An NTSTATUS error code indicating the problem.
 */
NTSTATUS WINAPI NtSetSystemTime(const LARGE_INTEGER *NewTime, LARGE_INTEGER *OldTime)
{
    TIME_FIELDS tf;
    struct timeval tv;
    struct timezone tz;
    struct tm t;
    time_t sec, oldsec;
    int dst, bias;
    int err;

    /* Return the old time if necessary */
    if(OldTime)
        NtQuerySystemTime(OldTime);

    RtlTimeToTimeFields(NewTime, &tf);

    /* call gettimeofday to get the current timezone */
    gettimeofday(&tv, &tz);
    oldsec = tv.tv_sec;
    /* get delta local time from utc */
    bias = TIME_GetBias(oldsec, &dst);

    /* get the number of seconds */
    t.tm_sec = tf.Second;
    t.tm_min = tf.Minute;
    t.tm_hour = tf.Hour;
    t.tm_mday = tf.Day;
    t.tm_mon = tf.Month - 1;
    t.tm_year = tf.Year - 1900;
    t.tm_isdst = dst;
    sec = mktime (&t);
    /* correct for timezone and daylight */
    sec += bias;

    /* set the new time */
    tv.tv_sec = sec;
    tv.tv_usec = tf.Milliseconds * 1000;

    /* error and sanity check*/
    if(sec == (time_t)-1 || abs((int)(sec-oldsec)) > SETTIME_MAX_ADJUST) {
        err = 2;
    } else {
#ifdef HAVE_SETTIMEOFDAY
        err = settimeofday(&tv, NULL); /* 0 is OK, -1 is error */
        if(err == 0)
            return STATUS_SUCCESS;
#else
        err = 1;
#endif
    }

    ERR("Cannot set time to %d/%d/%d %d:%d:%d Time adjustment %ld %s\n",
            tf.Year, tf.Month, tf.Day, tf.Hour, tf.Minute, tf.Second,
            (long)(sec-oldsec),
            err == -1 ? "No Permission"
                      : sec == (time_t)-1 ? "" : "is too large." );

    if(err == 2)
        return STATUS_INVALID_PARAMETER;
    else if(err == -1)
        return STATUS_PRIVILEGE_NOT_HELD;
    else
        return STATUS_NOT_IMPLEMENTED;
}
