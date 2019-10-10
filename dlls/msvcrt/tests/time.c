/*
 * Unit test suite for time functions.
 *
 * Copyright 2004 Uwe Bonnes
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

#include "wine/test.h"
#include "winbase.h"
#include "time.h"

#include <stdlib.h> /*setenv*/
#include <stdio.h> /*printf*/

#define SECSPERDAY         86400
#define SECSPERHOUR        3600
#define SECSPERMIN         60
#define MINSPERHOUR        60
#define HOURSPERDAY        24

static void test_gmtime()
{
    time_t gmt = (time_t)NULL;
    struct tm* gmt_tm = gmtime(&gmt);
    if(gmt_tm == 0)
	{
	    ok(0,"gmtime() error\n");
	    return;
	}
    ok(((gmt_tm->tm_year == 70) && (gmt_tm->tm_mon  == 0) && (gmt_tm->tm_yday  == 0) &&
	(gmt_tm->tm_mday ==  1) && (gmt_tm->tm_wday == 4) && (gmt_tm->tm_hour  == 0) &&
	(gmt_tm->tm_min  ==  0) && (gmt_tm->tm_sec  == 0) && (gmt_tm->tm_isdst == 0)),
       "Wrong date:Year %4d mon %2d yday %3d mday %2d wday %1d hour%2d min %2d sec %2d dst %2d\n",
       gmt_tm->tm_year, gmt_tm->tm_mon, gmt_tm->tm_yday, gmt_tm->tm_mday, gmt_tm->tm_wday, 
       gmt_tm->tm_hour, gmt_tm->tm_min, gmt_tm->tm_sec, gmt_tm->tm_isdst); 
  
}
static void test_mktime()
{
    TIME_ZONE_INFORMATION tzinfo;
    DWORD res =  GetTimeZoneInformation(&tzinfo);
    struct tm my_tm;
    time_t nulltime, local_time;
    char TZ_env[256];
    int secs;

    ok (res != 0, "GetTimeZoneInformation faile\n");
    /* Bias may be positive or negative, to use offset of one day */
    secs= SECSPERDAY - tzinfo.Bias*SECSPERMIN;
    my_tm.tm_mday = 1 + secs/SECSPERDAY;
    secs = secs % SECSPERDAY;
    my_tm.tm_hour = secs / SECSPERHOUR;
    secs = secs % SECSPERHOUR;
    my_tm.tm_min = secs / SECSPERMIN;
    secs = secs % SECSPERMIN;
    my_tm.tm_sec = secs;

    my_tm.tm_year = 70;
    my_tm.tm_mon  =  0;
    my_tm.tm_isdst=  0;
  
    local_time = mktime(&my_tm);
    ok(((DWORD)local_time == SECSPERDAY), "mktime returned 0x%08lx\n",(DWORD)local_time);

    /* TEST that we are indepentant from the TZ variable */
    /*Argh, msvcrt doesn't have setenv() */
    _snprintf(TZ_env,255,"TZ=%s",(getenv("TZ")?getenv("TZ"):""));
    putenv("TZ=GMT");
    nulltime = mktime(&my_tm);
    ok(((DWORD)nulltime == SECSPERDAY),"mktime returned 0x%08lx\n",(DWORD)nulltime);
    putenv(TZ_env);
}
static void test_localtime()
{
    TIME_ZONE_INFORMATION tzinfo;
    DWORD res =  GetTimeZoneInformation(&tzinfo);
    time_t gmt = (time_t)(SECSPERDAY + tzinfo.Bias*SECSPERMIN);
    char TZ_env[256];
    struct tm* lt;
    
    ok (res != 0, "GetTimeZoneInformation faile\n");
    lt = localtime(&gmt);
    ok(((lt->tm_year == 70) && (lt->tm_mon  == 0) && (lt->tm_yday  == 1) &&
	(lt->tm_mday ==  2) && (lt->tm_wday == 5) && (lt->tm_hour  == 0) &&
	(lt->tm_min  ==  0) && (lt->tm_sec  == 0) && (lt->tm_isdst == 0)),
       "Wrong date:Year %4d mon %2d yday %3d mday %2d wday %1d hour%2d min %2d sec %2d dst %2d\n",
       lt->tm_year, lt->tm_mon, lt->tm_yday, lt->tm_mday, lt->tm_wday, lt->tm_hour, 
       lt->tm_min, lt->tm_sec, lt->tm_isdst); 

    _snprintf(TZ_env,255,"TZ=%s",(getenv("TZ")?getenv("TZ"):""));
    putenv("TZ=GMT");
    lt = localtime(&gmt);
    ok(((lt->tm_year == 70) && (lt->tm_mon  == 0) && (lt->tm_yday  == 1) &&
	(lt->tm_mday ==  2) && (lt->tm_wday == 5) && (lt->tm_hour  == 0) &&
	(lt->tm_min  ==  0) && (lt->tm_sec  == 0) && (lt->tm_isdst == 0)),
       "Wrong date:Year %4d mon %2d yday %3d mday %2d wday %1d hour%2d min %2d sec %2d dst %2d\n",
       lt->tm_year, lt->tm_mon, lt->tm_yday, lt->tm_mday, lt->tm_wday, lt->tm_hour, 
       lt->tm_min, lt->tm_sec, lt->tm_isdst); 
    putenv(TZ_env);
}


START_TEST(time)
{
    test_gmtime();
    test_mktime();
    test_localtime();
}
