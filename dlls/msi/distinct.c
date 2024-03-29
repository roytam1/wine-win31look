/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002 Mike McCormack for CodeWeavers
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wine/debug.h"
#include "msi.h"
#include "msiquery.h"
#include "objbase.h"
#include "objidl.h"
#include "msipriv.h"
#include "winnls.h"

#include "query.h"

WINE_DEFAULT_DEBUG_CHANNEL(msi);

typedef struct tagDISTINCTSET
{
    UINT val;
    UINT count;
    UINT row;
    struct tagDISTINCTSET *nextrow;
    struct tagDISTINCTSET *nextcol;
} DISTINCTSET;

typedef struct tagMSIDISTINCTVIEW
{
    MSIVIEW        view;
    MSIDATABASE   *db;
    MSIVIEW       *table;
    UINT           row_count;
    UINT          *translation;
} MSIDISTINCTVIEW;

static DISTINCTSET ** distinct_insert( DISTINCTSET **x, UINT val, UINT row )
{
    /* horrible O(n) find */
    while( *x )
    {
        if( (*x)->val == val )
        {
            (*x)->count++;
            return x;
        }
        x = &(*x)->nextrow;
    }

    /* nothing found, so add one */
    *x = HeapAlloc( GetProcessHeap(), 0, sizeof (DISTINCTSET) );
    if( *x )
    {
        (*x)->val = val;
        (*x)->count = 1;
        (*x)->row = row;
        (*x)->nextrow = NULL;
        (*x)->nextcol = NULL;
    }
    return x;
}

static void distinct_free( DISTINCTSET *x )
{
    while( x )
    {
        DISTINCTSET *next = x->nextrow;
        distinct_free( x->nextcol );
        HeapFree( GetProcessHeap(), 0, x );
        x = next;
    }
}

static UINT DISTINCT_fetch_int( struct tagMSIVIEW *view, UINT row, UINT col, UINT *val )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p %d %d %p\n", dv, row, col, val );

    if( !dv->table )
        return ERROR_FUNCTION_FAILED;

    if( row >= dv->row_count )
        return ERROR_INVALID_PARAMETER;

    row = dv->translation[ row ];

    return dv->table->ops->fetch_int( dv->table, row, col, val );
}

static UINT DISTINCT_execute( struct tagMSIVIEW *view, MSIHANDLE record )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;
    UINT r, i, j, r_count, c_count;
    DISTINCTSET *rowset = NULL;

    TRACE("%p %ld\n", dv, record);

    if( !dv->table )
         return ERROR_FUNCTION_FAILED;

    r = dv->table->ops->execute( dv->table, record );
    if( r != ERROR_SUCCESS )
        return r;

    r = dv->table->ops->get_dimensions( dv->table, &r_count, &c_count );
    if( r != ERROR_SUCCESS )
        return r;

    dv->translation = HeapAlloc( GetProcessHeap(), 0, r_count*sizeof(UINT) );
    if( !dv->translation )
        return ERROR_FUNCTION_FAILED;

    /* build it */
    for( i=0; i<r_count; i++ )
    {
        DISTINCTSET **x = &rowset;

        for( j=1; j<=c_count; j++ )
        {
            UINT val = 0;
            r = dv->table->ops->fetch_int( dv->table, i, j, &val );
            if( r != ERROR_SUCCESS )
            {
                ERR("Failed to fetch int at %d %d\n", i, j );
                distinct_free( rowset );
                return r;
            }
            x = distinct_insert( x, val, i );
            if( !*x )
            {
                ERR("Failed to insert at %d %d\n", i, j );
                distinct_free( rowset );
                return ERROR_FUNCTION_FAILED;
            }
            if( j != c_count )
                x = &(*x)->nextcol;
        }

        /* check if it was distinct and if so, include it */
        if( (*x)->row == i )
        {
            TRACE("Row %d -> %d\n", dv->row_count, i);
            dv->translation[dv->row_count++] = i;
        }
    }

    distinct_free( rowset );

    return ERROR_SUCCESS;
}

static UINT DISTINCT_close( struct tagMSIVIEW *view )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p\n", dv );

    if( !dv->table )
         return ERROR_FUNCTION_FAILED;

    if( dv->translation )
        HeapFree( GetProcessHeap(), 0, dv->translation );
    dv->translation = NULL;
    dv->row_count = 0;

    return dv->table->ops->close( dv->table );
}

static UINT DISTINCT_get_dimensions( struct tagMSIVIEW *view, UINT *rows, UINT *cols )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p %p %p\n", dv, rows, cols );

    if( !dv->table )
        return ERROR_FUNCTION_FAILED;

    if( rows )
    {
        if( !dv->translation )
            return ERROR_FUNCTION_FAILED;
        *rows = dv->row_count;
    }

    return dv->table->ops->get_dimensions( dv->table, NULL, cols );
}

static UINT DISTINCT_get_column_info( struct tagMSIVIEW *view,
                UINT n, LPWSTR *name, UINT *type )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p %d %p %p\n", dv, n, name, type );

    if( !dv->table )
         return ERROR_FUNCTION_FAILED;

    return dv->table->ops->get_column_info( dv->table, n, name, type );
}

static UINT DISTINCT_modify( struct tagMSIVIEW *view, MSIMODIFY eModifyMode, MSIHANDLE hrec)
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p %d %ld\n", dv, eModifyMode, hrec );

    if( !dv->table )
         return ERROR_FUNCTION_FAILED;

    return dv->table->ops->modify( dv->table, eModifyMode, hrec );
}

static UINT DISTINCT_delete( struct tagMSIVIEW *view )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p\n", dv );

    if( dv->table )
        dv->table->ops->delete( dv->table );

    if( dv->translation )
        HeapFree( GetProcessHeap(), 0, dv->translation );
    HeapFree( GetProcessHeap(), 0, dv );

    return ERROR_SUCCESS;
}


MSIVIEWOPS distinct_ops =
{
    DISTINCT_fetch_int,
    NULL,
    NULL,
    DISTINCT_execute,
    DISTINCT_close,
    DISTINCT_get_dimensions,
    DISTINCT_get_column_info,
    DISTINCT_modify,
    DISTINCT_delete
};

UINT DISTINCT_CreateView( MSIDATABASE *db, MSIVIEW **view, MSIVIEW *table )
{
    MSIDISTINCTVIEW *dv = NULL;
    UINT count = 0, r;

    TRACE("%p\n", dv );

    r = table->ops->get_dimensions( table, NULL, &count );
    if( r != ERROR_SUCCESS )
    {
        ERR("can't get table dimensions\n");
        return r;
    }

    dv = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *dv );
    if( !dv )
        return ERROR_FUNCTION_FAILED;
    
    /* fill the structure */
    dv->view.ops = &distinct_ops;
    dv->db = db;
    dv->table = table;
    dv->translation = NULL;
    dv->row_count = 0;
    *view = (MSIVIEW*) dv;

    return ERROR_SUCCESS;
}
