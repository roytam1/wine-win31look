/*
 * Copyright (C) the Wine project
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

#ifndef __WINE_WINNLS_H
#define __WINE_WINNLS_H
#ifndef NONLS

#ifdef __cplusplus
extern "C" {
#endif

/* Country codes */
#define CTRY_DEFAULT            (0)
#define CTRY_ALBANIA            (355)
#define CTRY_ALGERIA            (213)
#define CTRY_ARGENTINA          (54)
#define CTRY_ARMENIA            (374)
#define CTRY_AUSTRALIA          (61)
#define CTRY_AUSTRIA            (43)
#define CTRY_AZERBAIJAN         (994)
#define CTRY_BAHRAIN            (973)
#define CTRY_BELARUS            (375)
#define CTRY_BELGIUM            (32)
#define CTRY_BELIZE             (501)
#define CTRY_BOLIVIA            (591)
#define CTRY_BRAZIL             (55)
#define CTRY_BRUNEI_DARUSSALAM  (673)
#define CTRY_BULGARIA           (359)
#define CTRY_CANADA             (2)
#define CTRY_CARIBBEAN          (1)
#define CTRY_CHILE              (56)
#define CTRY_COLOMBIA           (57)
#define CTRY_COSTA_RICA         (506)
#define CTRY_CROATIA            (385)
#define CTRY_CZECH              (420)
#define CTRY_DENMARK            (45)
#define CTRY_DOMINICAN_REPUBLIC (1)
#define CTRY_ECUADOR            (593)
#define CTRY_EGYPT              (20)
#define CTRY_EL_SALVADOR        (503)
#define CTRY_ESTONIA            (372)
#define CTRY_FAEROE_ISLANDS     (298)
#define CTRY_FINLAND            (358)
#define CTRY_FRANCE             (33)
#define CTRY_GEORGIA            (995)
#define CTRY_GERMANY            (49)
#define CTRY_GREECE             (30)
#define CTRY_GUATEMALA          (502)
#define CTRY_HONDURAS           (504)
#define CTRY_HONG_KONG          (852)
#define CTRY_HUNGARY            (36)
#define CTRY_ICELAND            (354)
#define CTRY_INDIA              (91)
#define CTRY_INDONESIA          (62)
#define CTRY_IRAN               (981)
#define CTRY_IRAQ               (964)
#define CTRY_IRELAND            (353)
#define CTRY_ISRAEL             (972)
#define CTRY_ITALY              (39)
#define CTRY_JAMAICA            (1)
#define CTRY_JAPAN              (81)
#define CTRY_JORDAN             (962)
#define CTRY_KAZAKSTAN          (7)
#define CTRY_KENYA              (254)
#define CTRY_KUWAIT             (965)
#define CTRY_KYRGYZSTAN         (996)
#define CTRY_LATVIA             (371)
#define CTRY_LEBANON            (961)
#define CTRY_LIBYA              (218)
#define CTRY_LIECHTENSTEIN      (41)
#define CTRY_LITHUANIA          (370)
#define CTRY_LUXEMBOURG         (352)
#define CTRY_MACAU              (853)
#define CTRY_MACEDONIA          (389)
#define CTRY_MALAYSIA           (60)
#define CTRY_MALDIVES           (960)
#define CTRY_MEXICO             (52)
#define CTRY_MONACO             (33)
#define CTRY_MONGOLIA           (976)
#define CTRY_MOROCCO            (212)
#define CTRY_NETHERLANDS        (31)
#define CTRY_NEW_ZEALAND        (64)
#define CTRY_NICARAGUA          (505)
#define CTRY_NORWAY             (47)
#define CTRY_OMAN               (968)
#define CTRY_PAKISTAN           (92)
#define CTRY_PANAMA             (507)
#define CTRY_PARAGUAY           (595)
#define CTRY_PERU               (51)
#define CTRY_PHILIPPINES        (63)
#define CTRY_POLAND             (48)
#define CTRY_PORTUGAL           (351)
#define CTRY_PRCHINA            (86)
#define CTRY_PUERTO_RICO        (1)
#define CTRY_QATAR              (974)
#define CTRY_ROMANIA            (40)
#define CTRY_RUSSIA             (7)
#define CTRY_SAUDI_ARABIA       (966)
#define CTRY_SERBIA             (381)
#define CTRY_SINGAPORE          (65)
#define CTRY_SLOVAK             (421)
#define CTRY_SLOVENIA           (386)
#define CTRY_SOUTH_AFRICA       (27)
#define CTRY_SOUTH_KOREA        (82)
#define CTRY_SPAIN              (34)
#define CTRY_SWEDEN             (46)
#define CTRY_SWITZERLAND        (41)
#define CTRY_SYRIA              (963)
#define CTRY_TAIWAN             (886)
#define CTRY_TATARSTAN          (7)
#define CTRY_THAILAND           (66)
#define CTRY_TRINIDAD_Y_TOBAGO  (1)
#define CTRY_TUNISIA            (216)
#define CTRY_TURKEY             (90)
#define CTRY_UAE                (971)
#define CTRY_UKRAINE            (380)
#define CTRY_UNITED_KINGDOM     (44)
#define CTRY_UNITED_STATES      (1)
#define CTRY_URUGUAY            (598)
#define CTRY_UZBEKISTAN         (7)
#define CTRY_VENEZUELA          (58)
#define CTRY_VIET_NAM           (84)
#define CTRY_YEMEN              (967)
#define CTRY_ZIMBABWE           (263)

#define MB_PRECOMPOSED              0x01
#define MB_COMPOSITE                0x02
#define MB_USEGLYPHCHARS            0x04
#define MB_ERR_INVALID_CHARS        0x08

#define LGRPID_INSTALLED            0x1
#define LGRPID_SUPPORTED            0x2

#define LCID_INSTALLED              0x1
#define LCID_SUPPORTED              0x2
#define LCID_ALTERNATE_SORTS        0x4

/* Locale flags */
#define	LOCALE_NOUSEROVERRIDE	    0x80000000
#define	LOCALE_USE_CP_ACP	    0x40000000
#define	LOCALE_RETURN_NUMBER	    0x20000000

/* Locale information types */
#define LOCALE_ILANGUAGE            0x0001
#define LOCALE_SLANGUAGE            0x0002
#define LOCALE_SENGLANGUAGE         0x1001
#define LOCALE_SABBREVLANGNAME      0x0003
#define LOCALE_SNATIVELANGNAME      0x0004
#define LOCALE_ICOUNTRY             0x0005
#define LOCALE_SCOUNTRY             0x0006
#define LOCALE_SENGCOUNTRY          0x1002
#define LOCALE_SABBREVCTRYNAME      0x0007
#define LOCALE_SNATIVECTRYNAME      0x0008
#define LOCALE_IDEFAULTLANGUAGE     0x0009
#define LOCALE_IDEFAULTCOUNTRY      0x000A
#define LOCALE_IDEFAULTCODEPAGE     0x000B
#define LOCALE_IDEFAULTANSICODEPAGE 0x1004
#define LOCALE_IDEFAULTMACCODEPAGE  0x1011
#define LOCALE_SLIST                0x000C
#define LOCALE_IMEASURE             0x000D
#define LOCALE_SDECIMAL             0x000E
#define LOCALE_STHOUSAND            0x000F
#define LOCALE_SGROUPING            0x0010
#define LOCALE_IDIGITS              0x0011
#define LOCALE_ILZERO               0x0012
#define LOCALE_INEGNUMBER           0x1010
#define LOCALE_SNATIVEDIGITS        0x0013
#define LOCALE_SCURRENCY            0x0014
#define LOCALE_SINTLSYMBOL          0x0015
#define LOCALE_SMONDECIMALSEP       0x0016
#define LOCALE_SMONTHOUSANDSEP      0x0017
#define LOCALE_SMONGROUPING         0x0018
#define LOCALE_ICURRDIGITS          0x0019
#define LOCALE_IINTLCURRDIGITS      0x001A
#define LOCALE_ICURRENCY            0x001B
#define LOCALE_INEGCURR             0x001C
#define LOCALE_SDATE                0x001D
#define LOCALE_STIME                0x001E
#define LOCALE_SSHORTDATE           0x001F
#define LOCALE_SLONGDATE            0x0020
#define LOCALE_STIMEFORMAT          0x1003
#define LOCALE_IDATE                0x0021
#define LOCALE_ILDATE               0x0022
#define LOCALE_ITIME                0x0023
#define LOCALE_ITIMEMARKPOSN        0x1005
#define LOCALE_ICENTURY             0x0024
#define LOCALE_ITLZERO              0x0025
#define LOCALE_IDAYLZERO            0x0026
#define LOCALE_IMONLZERO            0x0027
#define LOCALE_S1159                0x0028
#define LOCALE_S2359                0x0029
#define LOCALE_ICALENDARTYPE        0x1009
#define LOCALE_IOPTIONALCALENDAR    0x100B
#define LOCALE_IFIRSTDAYOFWEEK      0x100C
#define LOCALE_IFIRSTWEEKOFYEAR     0x100D
#define LOCALE_SDAYNAME1            0x002A
#define LOCALE_SDAYNAME2            0x002B
#define LOCALE_SDAYNAME3            0x002C
#define LOCALE_SDAYNAME4            0x002D
#define LOCALE_SDAYNAME5            0x002E
#define LOCALE_SDAYNAME6            0x002F
#define LOCALE_SDAYNAME7            0x0030
#define LOCALE_SABBREVDAYNAME1      0x0031
#define LOCALE_SABBREVDAYNAME2      0x0032
#define LOCALE_SABBREVDAYNAME3      0x0033
#define LOCALE_SABBREVDAYNAME4      0x0034
#define LOCALE_SABBREVDAYNAME5      0x0035
#define LOCALE_SABBREVDAYNAME6      0x0036
#define LOCALE_SABBREVDAYNAME7      0x0037
#define LOCALE_SMONTHNAME1          0x0038
#define LOCALE_SMONTHNAME2          0x0039
#define LOCALE_SMONTHNAME3          0x003A
#define LOCALE_SMONTHNAME4          0x003B
#define LOCALE_SMONTHNAME5          0x003C
#define LOCALE_SMONTHNAME6          0x003D
#define LOCALE_SMONTHNAME7          0x003E
#define LOCALE_SMONTHNAME8          0x003F
#define LOCALE_SMONTHNAME9          0x0040
#define LOCALE_SMONTHNAME10         0x0041
#define LOCALE_SMONTHNAME11         0x0042
#define LOCALE_SMONTHNAME12         0x0043
#define LOCALE_SMONTHNAME13         0x100E
#define LOCALE_SABBREVMONTHNAME1    0x0044
#define LOCALE_SABBREVMONTHNAME2    0x0045
#define LOCALE_SABBREVMONTHNAME3    0x0046
#define LOCALE_SABBREVMONTHNAME4    0x0047
#define LOCALE_SABBREVMONTHNAME5    0x0048
#define LOCALE_SABBREVMONTHNAME6    0x0049
#define LOCALE_SABBREVMONTHNAME7    0x004A
#define LOCALE_SABBREVMONTHNAME8    0x004B
#define LOCALE_SABBREVMONTHNAME9    0x004C
#define LOCALE_SABBREVMONTHNAME10   0x004D
#define LOCALE_SABBREVMONTHNAME11   0x004E
#define LOCALE_SABBREVMONTHNAME12   0x004F
#define LOCALE_SABBREVMONTHNAME13   0x100F
#define LOCALE_SPOSITIVESIGN        0x0050
#define LOCALE_SNEGATIVESIGN        0x0051
#define LOCALE_IPOSSIGNPOSN         0x0052
#define LOCALE_INEGSIGNPOSN         0x0053
#define LOCALE_IPOSSYMPRECEDES      0x0054
#define LOCALE_IPOSSEPBYSPACE       0x0055
#define LOCALE_INEGSYMPRECEDES      0x0056
#define LOCALE_INEGSEPBYSPACE       0x0057
#define	LOCALE_FONTSIGNATURE        0x0058
#define LOCALE_SISO639LANGNAME      0x0059
#define LOCALE_SISO3166CTRYNAME     0x005A

#define LOCALE_IDEFAULTEBCDICCODEPAGE 0x1012
#define LOCALE_IPAPERSIZE             0x100A
#define LOCALE_SENGCURRNAME           0x1007
#define LOCALE_SNATIVECURRNAME        0x1008
#define LOCALE_SYEARMONTH             0x1006
#define LOCALE_SSORTNAME              0x1013
#define LOCALE_IDIGITSUBSTITUTION     0x1014

#define LOCALE_IDEFAULTUNIXCODEPAGE   0x1030 /* Wine extension */

#define NORM_IGNORECASE     0x00001
#define NORM_IGNORENONSPACE 0x00002
#define NORM_IGNORESYMBOLS  0x00004
#define NORM_STRINGSORT     0x01000
#define NORM_IGNOREKANATYPE 0x10000
#define NORM_IGNOREWIDTH    0x20000

#define CP_ACP        0
#define CP_OEMCP      1
#define CP_MACCP      2
#define CP_THREAD_ACP 3
#define CP_SYMBOL     42
#define CP_UTF7       65000
#define CP_UTF8       65001

#define CP_UNIXCP     65010 /* Wine extension */

#define CP_INSTALLED 0x1
#define CP_SUPPORTED 0x2

#define WC_DISCARDNS         0x0010
#define WC_SEPCHARS          0x0020
#define WC_DEFAULTCHAR       0x0040
#define WC_COMPOSITECHECK    0x0200
#define WC_NO_BEST_FIT_CHARS 0x0400

#define MAP_FOLDCZONE        0x0010
#define MAP_PRECOMPOSED      0x0020
#define MAP_COMPOSITE        0x0040
#define MAP_FOLDDIGITS       0x0080
#define MAP_EXPAND_LIGATURES 0x2000


/* String mapping flags */
#define LCMAP_LOWERCASE  0x00000100	/* Make lower-case */
#define LCMAP_UPPERCASE  0x00000200	/* Make upper-case */
#define LCMAP_SORTKEY    0x00000400	/* Create a sort key */
#define LCMAP_BYTEREV    0x00000800	/* Reverse the result */

#define SORT_STRINGSORT  0x00001000	/* Take punctuation into account */

#define LCMAP_HIRAGANA   0x00100000	/* Transform Japanese katakana into hiragana */
#define LCMAP_KATAKANA   0x00200000	/* Transform Japanese hiragana into katakana */
#define LCMAP_HALFWIDTH  0x00400000	/* Use single byte chars in output */
#define LCMAP_FULLWIDTH  0x00800000	/* Use double byte chars in output */

#define LCMAP_LINGUISTIC_CASING   0x01000000 /* Change case by using language context */
#define LCMAP_SIMPLIFIED_CHINESE  0x02000000 /* Transform Chinese traditional into simplified */
#define LCMAP_TRADITIONAL_CHINESE 0x04000000 /* Transform Chinese simplified into traditional */

/* Date and time formatting flags */
#define DATE_SHORTDATE          0x01  /* Short date format */
#define DATE_LONGDATE           0x02  /* Long date format */
#define DATE_USE_ALT_CALENDAR   0x04  /* Use an Alternate calendar */
#define DATE_YEARMONTH          0x08  /* Year/month format */
#define DATE_LTRREADING         0x10  /* Add LTR reading marks */
#define DATE_RTLREADING         0x20  /* Add RTL reading marks */

#define TIME_FORCE24HOURFORMAT  0x08  /* Always use 24 hour clock */
#define TIME_NOTIMEMARKER       0x04  /* show no AM/PM */
#define TIME_NOSECONDS          0x02  /* show no seconds */
#define TIME_NOMINUTESORSECONDS 0x01  /* show no minutes either */

/* Unicode char type flags */
#define	CT_CTYPE1		0x0001	/* usual ctype */
#define	CT_CTYPE2		0x0002	/* bidirectional layout info */
#define	CT_CTYPE3		0x0004	/* textprocessing info */

/* Type 1 flags */
#define C1_UPPER		0x0001
#define C1_LOWER		0x0002
#define C1_DIGIT		0x0004
#define C1_SPACE		0x0008
#define C1_PUNCT		0x0010
#define C1_CNTRL		0x0020
#define C1_BLANK		0x0040
#define C1_XDIGIT		0x0080
#define C1_ALPHA		0x0100

/* Type 2 flags */
#define	C2_LEFTTORIGHT		0x0001
#define	C2_RIGHTTOLEFT		0x0002
#define	C2_EUROPENUMBER		0x0003
#define	C2_EUROPESEPARATOR	0x0004
#define	C2_EUROPETERMINATOR	0x0005
#define	C2_ARABICNUMBER		0x0006
#define	C2_COMMONSEPARATOR	0x0007
#define	C2_BLOCKSEPARATOR	0x0008
#define	C2_SEGMENTSEPARATOR	0x0009
#define	C2_WHITESPACE		0x000A
#define	C2_OTHERNEUTRAL		0x000B
#define	C2_NOTAPPLICABLE	0x0000

/* Type 3 flags */
#define	C3_NONSPACING		0x0001
#define	C3_DIACRITIC		0x0002
#define	C3_VOWELMARK		0x0004
#define	C3_SYMBOL		0x0008
#define	C3_KATAKANA		0x0010
#define	C3_HIRAGANA		0x0020
#define	C3_HALFWIDTH		0x0040
#define	C3_FULLWIDTH		0x0080
#define	C3_IDEOGRAPH		0x0100
#define	C3_KASHIDA		0x0200
#define	C3_LEXICAL		0x0400
#define	C3_ALPHA		0x8000
#define	C3_NOTAPPLICABLE	0x0000

/* Code page information.
 */
#define MAX_LEADBYTES     12
#define MAX_DEFAULTCHAR   2

/* Defines for calendar handling */
#define CAL_NOUSEROVERRIDE     LOCALE_NOUSEROVERRIDE
#define CAL_USE_CP_ACP         LOCALE_USE_CP_ACP
#define CAL_RETURN_NUMBER      LOCALE_RETURN_NUMBER

#define CAL_ICALINTVALUE       0x01
#define CAL_SCALNAME           0x02
#define CAL_IYEAROFFSETRANGE   0x03
#define CAL_SERASTRING         0x04
#define CAL_SSHORTDATE         0x05
#define CAL_SLONGDATE          0x06
#define CAL_SDAYNAME1          0x07
#define CAL_SDAYNAME2          0x08
#define CAL_SDAYNAME3          0x09
#define CAL_SDAYNAME4          0x0a
#define CAL_SDAYNAME5          0x0b
#define CAL_SDAYNAME6          0x0c
#define CAL_SDAYNAME7          0x0d
#define CAL_SABBREVDAYNAME1    0x0e
#define CAL_SABBREVDAYNAME2    0x0f
#define CAL_SABBREVDAYNAME3    0x10
#define CAL_SABBREVDAYNAME4    0x11
#define CAL_SABBREVDAYNAME5    0x12
#define CAL_SABBREVDAYNAME6    0x13
#define CAL_SABBREVDAYNAME7    0x14
#define CAL_SMONTHNAME1        0x15
#define CAL_SMONTHNAME2        0x16
#define CAL_SMONTHNAME3        0x17
#define CAL_SMONTHNAME4        0x18
#define CAL_SMONTHNAME5        0x19
#define CAL_SMONTHNAME6        0x1a
#define CAL_SMONTHNAME7        0x1b
#define CAL_SMONTHNAME8        0x1c
#define CAL_SMONTHNAME9        0x1d
#define CAL_SMONTHNAME10       0x1e
#define CAL_SMONTHNAME11       0x1f
#define CAL_SMONTHNAME12       0x20
#define CAL_SMONTHNAME13       0x21
#define CAL_SABBREVMONTHNAME1  0x22
#define CAL_SABBREVMONTHNAME2  0x23
#define CAL_SABBREVMONTHNAME3  0x24
#define CAL_SABBREVMONTHNAME4  0x25
#define CAL_SABBREVMONTHNAME5  0x26
#define CAL_SABBREVMONTHNAME6  0x27
#define CAL_SABBREVMONTHNAME7  0x28
#define CAL_SABBREVMONTHNAME8  0x29
#define CAL_SABBREVMONTHNAME9  0x2a
#define CAL_SABBREVMONTHNAME10 0x2b
#define CAL_SABBREVMONTHNAME11 0x2c
#define CAL_SABBREVMONTHNAME12 0x2d
#define CAL_SABBREVMONTHNAME13 0x2e
#define CAL_SYEARMONTH         0x2f
#define CAL_ITWODIGITYEARMAX   0x30

/* Calendar types */
#define CAL_GREGORIAN              1
#define CAL_GREGORIAN_US           2
#define CAL_JAPAN                  3
#define CAL_TAIWAN                 4
#define CAL_KOREA                  5
#define CAL_HIJRI                  6
#define CAL_THAI                   7
#define CAL_HEBREW                 8
#define CAL_GREGORIAN_ME_FRENCH    9
#define CAL_GREGORIAN_ARABIC       10
#define CAL_GREGORIAN_XLIT_ENGLISH 11
#define CAL_GREGORIAN_XLIT_FRENCH  12

/* EnumCalendarInfo Flags */
#define ENUM_ALL_CALENDARS 0xffffffff /* Enumerate all calendars within a locale */

/* CompareString results */
#define CSTR_LESS_THAN    1
#define CSTR_EQUAL        2
#define CSTR_GREATER_THAN 3

/*
 * Language Group IDs.
 * Resources in kernel32 are LGRPID_xxx+0x2000 because low values were used by LOCALE_xxx
 * This is done because resources in win2k kernel32 / winxp kernel32 are not even
 * stored the same way.
 */
#define LGRPID_WESTERN_EUROPE      0x01 /* Includes US and Africa */
#define LGRPID_CENTRAL_EUROPE      0x02
#define LGRPID_BALTIC              0x03
#define LGRPID_GREEK               0x04
#define LGRPID_CYRILLIC            0x05
#define LGRPID_TURKISH             0x06
#define LGRPID_JAPANESE            0x07
#define LGRPID_KOREAN              0x08
#define LGRPID_TRADITIONAL_CHINESE 0x09
#define LGRPID_SIMPLIFIED_CHINESE  0x0A
#define LGRPID_THAI                0x0B
#define LGRPID_HEBREW              0x0C
#define LGRPID_ARABIC              0x0D
#define LGRPID_VIETNAMESE          0x0E
#define LGRPID_INDIC               0x0F
#define LGRPID_GEORGIAN            0x10
#define LGRPID_ARMENIAN            0x11

/* Types
 */

typedef DWORD CALID;
typedef DWORD CALTYPE;
typedef LONG  GEOID;
typedef DWORD GEOCLASS;
typedef DWORD GEOTYPE;
typedef DWORD LCTYPE;
typedef DWORD LGRPID;

typedef struct
{
    UINT MaxCharSize;
    BYTE DefaultChar[MAX_DEFAULTCHAR];
    BYTE LeadByte[MAX_LEADBYTES];
} CPINFO, *LPCPINFO;

typedef struct
{
    UINT MaxCharSize;
    BYTE DefaultChar[MAX_DEFAULTCHAR];
    BYTE LeadByte[MAX_LEADBYTES];
    WCHAR UnicodeDefaultChar;
    UINT CodePage;
    CHAR CodePageName[MAX_PATH];
} CPINFOEXA, *LPCPINFOEXA;

typedef struct
{
    UINT MaxCharSize;
    BYTE DefaultChar[MAX_DEFAULTCHAR];
    BYTE LeadByte[MAX_LEADBYTES];
    WCHAR UnicodeDefaultChar;
    UINT CodePage;
    WCHAR CodePageName[MAX_PATH];
} CPINFOEXW, *LPCPINFOEXW;

DECL_WINELIB_TYPE_AW(CPINFOEX)
DECL_WINELIB_TYPE_AW(LPCPINFOEX)

typedef struct _numberfmtA {
    UINT NumDigits;
    UINT LeadingZero;
    UINT Grouping;
    LPSTR lpDecimalSep;
    LPSTR lpThousandSep;
    UINT NegativeOrder;
} NUMBERFMTA, *LPNUMBERFMTA;

typedef struct _numberfmtW {
    UINT NumDigits;
    UINT LeadingZero;
    UINT Grouping;
    LPWSTR lpDecimalSep;
    LPWSTR lpThousandSep;
    UINT NegativeOrder;
} NUMBERFMTW, *LPNUMBERFMTW;

DECL_WINELIB_TYPE_AW(NUMBERFMT)
DECL_WINELIB_TYPE_AW(LPNUMBERFMT)

typedef struct _currencyfmtA
{
    UINT NumDigits;
    UINT LeadingZero;
    UINT Grouping;
    LPSTR lpDecimalSep;
    LPSTR lpThousandSep;
    UINT NegativeOrder;
    UINT PositiveOrder;
    LPSTR lpCurrencySymbol;
} CURRENCYFMTA, *LPCURRENCYFMTA;

typedef struct _currencyfmtW
{
    UINT NumDigits;
    UINT LeadingZero;
    UINT Grouping;
    LPWSTR lpDecimalSep;
    LPWSTR lpThousandSep;
    UINT NegativeOrder;
    UINT PositiveOrder;
    LPWSTR lpCurrencySymbol;
} CURRENCYFMTW, *LPCURRENCYFMTW;

DECL_WINELIB_TYPE_AW(CURRENCYFMT)
DECL_WINELIB_TYPE_AW(LPCURRENCYFMT)


/* Define a bunch of callback types */

#if defined(STRICT)
typedef BOOL    (CALLBACK *CALINFO_ENUMPROCA)(LPSTR);
typedef BOOL    (CALLBACK *CALINFO_ENUMPROCW)(LPWSTR);
typedef BOOL    (CALLBACK *CALINFO_ENUMPROCEXA)(LPSTR,CALID);
typedef BOOL    (CALLBACK *CALINFO_ENUMPROCEXW)(LPWSTR,CALID);
typedef BOOL    (CALLBACK *CODEPAGE_ENUMPROCA)(LPSTR);
typedef BOOL    (CALLBACK *CODEPAGE_ENUMPROCW)(LPWSTR);
typedef BOOL    (CALLBACK *DATEFMT_ENUMPROCA)(LPSTR);
typedef BOOL    (CALLBACK *DATEFMT_ENUMPROCW)(LPWSTR);
typedef BOOL    (CALLBACK *DATEFMT_ENUMPROCEXA)(LPSTR,CALID);
typedef BOOL    (CALLBACK *DATEFMT_ENUMPROCEXW)(LPWSTR,CALID);
typedef BOOL    (CALLBACK *GEO_ENUMPROC)(GEOID);
typedef BOOL    (CALLBACK *LANGGROUPLOCALE_ENUMPROCA)(LGRPID,LCID,LPSTR,LONG_PTR);
typedef BOOL    (CALLBACK *LANGGROUPLOCALE_ENUMPROCW)(LGRPID,LCID,LPWSTR,LONG_PTR);
typedef BOOL    (CALLBACK *LANGUAGEGROUP_ENUMPROCA)(LGRPID,LPSTR,LPSTR,DWORD,LONG_PTR);
typedef BOOL    (CALLBACK *LANGUAGEGROUP_ENUMPROCW)(LGRPID,LPWSTR,LPWSTR,DWORD,LONG_PTR);
typedef BOOL    (CALLBACK *LOCALE_ENUMPROCA)(LPSTR);
typedef BOOL    (CALLBACK *LOCALE_ENUMPROCW)(LPWSTR);
typedef BOOL    (CALLBACK *TIMEFMT_ENUMPROCA)(LPSTR);
typedef BOOL    (CALLBACK *TIMEFMT_ENUMPROCW)(LPWSTR);
typedef BOOL    (CALLBACK *UILANGUAGE_ENUMPROCA)(LPSTR,LONG_PTR);
typedef BOOL    (CALLBACK *UILANGUAGE_ENUMPROCW)(LPWSTR,LONG_PTR);
#else
typedef FARPROC CALINFO_ENUMPROCA;
typedef FARPROC CALINFO_ENUMPROCW;
typedef FARPROC CALINFO_ENUMPROCEXA;
typedef FARPROC CALINFO_ENUMPROCEXW;
typedef FARPROC CODEPAGE_ENUMPROCA;
typedef FARPROC CODEPAGE_ENUMPROCW;
typedef FARPROC DATEFMT_ENUMPROCA;
typedef FARPROC DATEFMT_ENUMPROCW;
typedef FARPROC DATEFMT_ENUMPROCEXA;
typedef FARPROC DATEFMT_ENUMPROCEXW;
typedef FARPROC GEO_ENUMPROC;
typedef FARPROC LANGGROUPLOCALE_ENUMPROCA;
typedef FARPROC LANGGROUPLOCALE_ENUMPROCW;
typedef FARPROC LANGUAGEGROUP_ENUMPROCA;
typedef FARPROC LANGUAGEGROUP_ENUMPROCW;
typedef FARPROC LOCALE_ENUMPROCA;
typedef FARPROC LOCALE_ENUMPROCW;
typedef FARPROC TIMEFMT_ENUMPROCA;
typedef FARPROC TIMEFMT_ENUMPROCW;
typedef FARPROC UILANGUAGE_ENUMPROCA;
typedef FARPROC UILANGUAGE_ENUMPROCW;
#endif /* STRICT */

DECL_WINELIB_TYPE_AW(CALINFO_ENUMPROC)
DECL_WINELIB_TYPE_AW(CALINFO_ENUMPROCEX)
DECL_WINELIB_TYPE_AW(CODEPAGE_ENUMPROC)
DECL_WINELIB_TYPE_AW(DATEFMT_ENUMPROC)
DECL_WINELIB_TYPE_AW(DATEFMT_ENUMPROCEX)
DECL_WINELIB_TYPE_AW(LANGGROUPLOCALE_ENUMPROC)
DECL_WINELIB_TYPE_AW(LANGUAGEGROUP_ENUMPROC)
DECL_WINELIB_TYPE_AW(LOCALE_ENUMPROC)
DECL_WINELIB_TYPE_AW(TIMEFMT_ENUMPROC)
DECL_WINELIB_TYPE_AW(UILANGUAGE_ENUMPROC)

/* Geographic Information types */
enum SYSGEOTYPE
{
    GEO_NATION = 1,
    GEO_LATITUDE,
    GEO_LONGITUDE,
    GEO_ISO2,
    GEO_ISO3,
    GEO_RFC1766,
    GEO_LCID,
    GEO_FRIENDLYNAME,
    GEO_OFFICIALNAME,
    GEO_TIMEZONES,
    GEO_OFFICIALLANGUAGES
};

enum SYSGEOCLASS
{
    GEOCLASS_REGION = 14,
    GEOCLASS_NATION = 16
};

#define GEOID_NOT_AVAILABLE (-1)


/* NLS Functions.
 */

INT         WINAPI CompareStringA(LCID,DWORD,LPCSTR,INT,LPCSTR,INT);
INT         WINAPI CompareStringW(LCID,DWORD,LPCWSTR,INT,LPCWSTR,INT);
#define     CompareString WINELIB_NAME_AW(CompareString)
LCID        WINAPI ConvertDefaultLocale(LCID);
BOOL        WINAPI EnumCalendarInfoA(CALINFO_ENUMPROCA,LCID,CALID,CALTYPE);
BOOL        WINAPI EnumCalendarInfoW(CALINFO_ENUMPROCW,LCID,CALID,CALTYPE);
#define     EnumCalendarInfo WINELIB_NAME_AW(EnumCalendarInfo)
BOOL        WINAPI EnumCalendarInfoExA(CALINFO_ENUMPROCEXA,LCID,CALID,CALTYPE);
BOOL        WINAPI EnumCalendarInfoExW(CALINFO_ENUMPROCEXW,LCID,CALID,CALTYPE);
#define     EnumCalendarInfoEx WINELIB_NAME_AW(EnumCalendarInfoEx)
BOOL        WINAPI EnumDateFormatsA(DATEFMT_ENUMPROCA,LCID,DWORD);
BOOL        WINAPI EnumDateFormatsW(DATEFMT_ENUMPROCW,LCID,DWORD);
#define     EnumDateFormats WINELIB_NAME_AW(EnumDateFormats)
BOOL        WINAPI EnumDateFormatsExA(DATEFMT_ENUMPROCEXA,LCID,DWORD);
BOOL        WINAPI EnumDateFormatsExW(DATEFMT_ENUMPROCEXW,LCID,DWORD);
#define     EnumDateFormatsEx WINELIB_NAME_AW(EnumDateFormatsEx)
BOOL        WINAPI EnumSystemCodePagesA(CODEPAGE_ENUMPROCA,DWORD);
BOOL        WINAPI EnumSystemCodePagesW(CODEPAGE_ENUMPROCW,DWORD);
#define     EnumSystemCodePages WINELIB_NAME_AW(EnumSystemCodePages)
BOOL        WINAPI EnumSystemGeoID(GEOCLASS,GEOID,GEO_ENUMPROC);
BOOL        WINAPI EnumSystemLocalesA(LOCALE_ENUMPROCA,DWORD);
BOOL        WINAPI EnumSystemLocalesW(LOCALE_ENUMPROCW,DWORD);
#define     EnumSystemLocales WINELIB_NAME_AW(EnumSystemLocales)
BOOL        WINAPI EnumSystemLanguageGroupsA(LANGUAGEGROUP_ENUMPROCA,DWORD,LONG_PTR);
BOOL        WINAPI EnumSystemLanguageGroupsW(LANGUAGEGROUP_ENUMPROCW,DWORD,LONG_PTR);
#define     EnumSystemLanguageGroups WINELIB_NAME_AW(EnumSystemLanguageGroups)
BOOL        WINAPI EnumLanguageGroupLocalesA(LANGGROUPLOCALE_ENUMPROCA,LGRPID,DWORD,LONG_PTR);
BOOL        WINAPI EnumLanguageGroupLocalesW(LANGGROUPLOCALE_ENUMPROCW,LGRPID,DWORD,LONG_PTR);
#define     EnumLanguageGroupLocales WINELIB_NAME_AW(EnumLanguageGroupLocales)
BOOL        WINAPI EnumTimeFormatsA(TIMEFMT_ENUMPROCA,LCID,DWORD);
BOOL        WINAPI EnumTimeFormatsW(TIMEFMT_ENUMPROCW,LCID,DWORD);
#define     EnumTimeFormats WINELIB_NAME_AW(EnumTimeFormats)
BOOL        WINAPI EnumUILanguagesA(UILANGUAGE_ENUMPROCA,DWORD,LONG_PTR);
BOOL        WINAPI EnumUILanguagesW(UILANGUAGE_ENUMPROCW,DWORD,LONG_PTR);
#define     EnumUILanguages WINELIB_NAME_AW(EnumUILanguages)
INT         WINAPI FoldStringA(DWORD,LPCSTR,INT,LPSTR,INT);
INT         WINAPI FoldStringW(DWORD,LPCWSTR,INT,LPWSTR,INT);
#define     FoldString WINELIB_NAME_AW(FoldString)
UINT        WINAPI GetACP(void);
BOOL        WINAPI GetCPInfo(UINT,LPCPINFO);
BOOL        WINAPI GetCPInfoExA(UINT,DWORD,LPCPINFOEXA);
BOOL        WINAPI GetCPInfoExW(UINT,DWORD,LPCPINFOEXW);
#define     GetCPInfoEx WINELIB_NAME_AW(GetCPInfoEx)
INT         WINAPI GetCalendarInfoA(LCID,DWORD,DWORD,LPSTR,INT,LPDWORD);
INT         WINAPI GetCalendarInfoW(LCID,DWORD,DWORD,LPWSTR,INT,LPDWORD);
#define     GetCalendarInfo WINELIB_NAME_AW(GetCalendarInfo)
INT         WINAPI GetCurrencyFormatA(LCID,DWORD,LPCSTR,const CURRENCYFMTA*,LPSTR,INT);
INT         WINAPI GetCurrencyFormatW(LCID,DWORD,LPCWSTR,const CURRENCYFMTW*,LPWSTR,INT);
#define     GetCurrencyFormat WINELIB_NAME_AW(GetCurrencyFormat)
INT         WINAPI GetDateFormatA(LCID,DWORD,const SYSTEMTIME*,LPCSTR,LPSTR,INT);
INT         WINAPI GetDateFormatW(LCID,DWORD,const SYSTEMTIME*,LPCWSTR,LPWSTR,INT);
#define     GetDateFormat WINELIB_NAME_AW(GetDateFormat)
INT         WINAPI GetGeoInfoA(GEOID,GEOTYPE,LPSTR,INT,LANGID);
INT         WINAPI GetGeoInfoW(GEOID,GEOTYPE,LPWSTR,INT,LANGID);
#define     GetGeoInfo WINELIB_NAME_AW(GetGeoInfo)
INT         WINAPI GetLocaleInfoA(LCID,LCTYPE,LPSTR,INT);
INT         WINAPI GetLocaleInfoW(LCID,LCTYPE,LPWSTR,INT);
#define     GetLocaleInfo WINELIB_NAME_AW(GetLocaleInfo)
INT         WINAPI GetNumberFormatA(LCID,DWORD,LPCSTR,const NUMBERFMTA*,LPSTR,INT);
INT         WINAPI GetNumberFormatW(LCID,DWORD,LPCWSTR,const NUMBERFMTW*,LPWSTR,INT);
#define     GetNumberFormat WINELIB_NAME_AW(GetNumberFormat)
UINT        WINAPI GetOEMCP(void);
BOOL        WINAPI GetStringTypeA(LCID,DWORD,LPCSTR,INT,LPWORD);
BOOL        WINAPI GetStringTypeW(DWORD,LPCWSTR,INT,LPWORD);
BOOL        WINAPI GetStringTypeExA(LCID,DWORD,LPCSTR,INT,LPWORD);
BOOL        WINAPI GetStringTypeExW(LCID,DWORD,LPCWSTR,INT,LPWORD);
#define     GetStringTypeEx WINELIB_NAME_AW(GetStringTypeEx)
LANGID      WINAPI GetSystemDefaultLangID(void);
LCID        WINAPI GetSystemDefaultLCID(void);
LANGID      WINAPI GetSystemDefaultUILanguage(void);
LCID        WINAPI GetThreadLocale(void);
INT         WINAPI GetTimeFormatA(LCID,DWORD,const SYSTEMTIME*,LPCSTR,LPSTR,INT);
INT         WINAPI GetTimeFormatW(LCID,DWORD,const SYSTEMTIME*,LPCWSTR,LPWSTR,INT);
#define     GetTimeFormat WINELIB_NAME_AW(GetTimeFormat)
LANGID      WINAPI GetUserDefaultLangID(void);
LCID        WINAPI GetUserDefaultLCID(void);
LANGID      WINAPI GetUserDefaultUILanguage(void);
GEOID       WINAPI GetUserGeoID(GEOCLASS);
BOOL        WINAPI IsDBCSLeadByte(BYTE);
BOOL        WINAPI IsDBCSLeadByteEx(UINT,BYTE);
BOOL        WINAPI IsValidCodePage(UINT);
BOOL        WINAPI IsValidLocale(LCID,DWORD);
BOOL        WINAPI IsValidLanguageGroup(LGRPID,DWORD);
INT         WINAPI LCMapStringA(LCID,DWORD,LPCSTR,INT,LPSTR,INT);
INT         WINAPI LCMapStringW(LCID,DWORD,LPCWSTR,INT,LPWSTR,INT);
#define     LCMapString WINELIB_NAME_AW(LCMapString)
INT         WINAPI MultiByteToWideChar(UINT,DWORD,LPCSTR,INT,LPWSTR,INT);
INT         WINAPI SetCalendarInfoA(LCID,CALID,CALTYPE,LPCSTR);
INT         WINAPI SetCalendarInfoW(LCID,CALID,CALTYPE,LPCWSTR);
#define     SetCalendarInfo WINELIB_NAME_AW(SetCalendarInfo)
BOOL        WINAPI SetLocaleInfoA(LCID,LCTYPE,LPCSTR);
BOOL        WINAPI SetLocaleInfoW(LCID,LCTYPE,LPCWSTR);
#define     SetLocaleInfo WINELIB_NAME_AW(SetLocaleInfo)
BOOL        WINAPI SetThreadLocale(LCID);
BOOL        WINAPI SetUserGeoID(GEOID);
INT         WINAPI WideCharToMultiByte(UINT,DWORD,LPCWSTR,INT,LPSTR,INT,LPCSTR,LPBOOL);

#ifdef __cplusplus
}
#endif

#endif /* !NONLS */
#endif  /* __WINE_WINNLS_H */
