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


/* below is the query interface to a table */

typedef struct tagMSIWHEREVIEW
{
    MSIVIEW        view;
    MSIDATABASE   *db;
    MSIVIEW       *table;
    UINT           row_count;
    UINT          *reorder;
    struct expr   *cond;
} MSIWHEREVIEW;

static UINT WHERE_fetch_int( struct tagMSIVIEW *view, UINT row, UINT col, UINT *val )
{
    MSIWHEREVIEW *wv = (MSIWHEREVIEW*)view;

    TRACE("%p %d %d %p\n", wv, row, col, val );

    if( !wv->table )
        return ERROR_FUNCTION_FAILED;

    if( row > wv->row_count )
        return ERROR_NO_MORE_ITEMS;

    row = wv->reorder[ row ];

    return wv->table->ops->fetch_int( wv->table, row, col, val );
}

static UINT INT_evaluate( UINT lval, UINT op, UINT rval )
{
    switch( op )
    {
    case OP_EQ:
        return ( lval == rval );
    case OP_AND:
        return ( lval && rval );
    case OP_OR:
        return ( lval || rval );
    case OP_GT:
        return ( lval > rval );
    case OP_LT:
        return ( lval < rval );
    case OP_LE:
        return ( lval <= rval );
    case OP_GE:
        return ( lval >= rval );
    case OP_NE:
        return ( lval != rval );
    case OP_ISNULL:
        return ( !lval );
    case OP_NOTNULL:
        return ( lval );
    default:
        ERR("Unknown operator %d\n", op );
    }
    return 0;
}

static UINT WHERE_evaluate( MSIVIEW *table, UINT row, 
                             struct expr *cond, UINT *val )
{
    UINT r, lval, rval;

    if( !cond )
        return ERROR_SUCCESS;

    switch( cond->type )
    {
    case EXPR_COL_NUMBER:
        return table->ops->fetch_int( table, row, cond->u.col_number, val );

    /* case EXPR_IVAL:
        *val = cond->u.ival;
        return ERROR_SUCCESS; */

    case EXPR_UVAL:
        *val = cond->u.uval;
        return ERROR_SUCCESS;

    case EXPR_COMPLEX:
        r = WHERE_evaluate( table, row, cond->u.expr.left, &lval );
        if( r != ERROR_SUCCESS )
            return r;
        r = WHERE_evaluate( table, row, cond->u.expr.right, &rval );
        if( r != ERROR_SUCCESS )
            return r;
        *val = INT_evaluate( lval, cond->u.expr.op, rval );
        return ERROR_SUCCESS;

    default:
        ERR("Invalid expression type\n");
        break;
    } 

    return ERROR_SUCCESS;

}

static UINT WHERE_execute( struct tagMSIVIEW *view, MSIHANDLE record )
{
    MSIWHEREVIEW *wv = (MSIWHEREVIEW*)view;
    UINT count = 0, r, val, i;
    MSIVIEW *table = wv->table;

    TRACE("%p %ld\n", wv, record);

    if( !table )
         return ERROR_FUNCTION_FAILED;

    r = table->ops->execute( table, record );
    if( r != ERROR_SUCCESS )
        return r;

    r = table->ops->get_dimensions( table, &count, NULL );
    if( r != ERROR_SUCCESS )
        return r;

    wv->reorder = HeapAlloc( GetProcessHeap(), 0, count*sizeof(UINT) );
    if( !wv->reorder )
        return ERROR_FUNCTION_FAILED;

    for( i=0; i<count; i++ )
    {
        val = 0;
        r = WHERE_evaluate( table, i, wv->cond, &val );
        if( r != ERROR_SUCCESS )
            return r;
        if( val )
            wv->reorder[ wv->row_count ++ ] = i;
    }

    return ERROR_SUCCESS;
}

static UINT WHERE_close( struct tagMSIVIEW *view )
{
    MSIWHEREVIEW *wv = (MSIWHEREVIEW*)view;

    TRACE("%p\n", wv );

    if( !wv->table )
         return ERROR_FUNCTION_FAILED;

    if( wv->reorder )
        HeapFree( GetProcessHeap(), 0, wv->reorder );
    wv->reorder = NULL;

    return wv->table->ops->close( wv->table );
}

static UINT WHERE_get_dimensions( struct tagMSIVIEW *view, UINT *rows, UINT *cols )
{
    MSIWHEREVIEW *wv = (MSIWHEREVIEW*)view;

    TRACE("%p %p %p\n", wv, rows, cols );

    if( !wv->table )
         return ERROR_FUNCTION_FAILED;

    if( rows )
    {
        if( !wv->reorder )
            return ERROR_FUNCTION_FAILED;
        *rows = wv->row_count;
    }

    return wv->table->ops->get_dimensions( wv->table, NULL, cols );
}

static UINT WHERE_get_column_info( struct tagMSIVIEW *view,
                UINT n, LPWSTR *name, UINT *type )
{
    MSIWHEREVIEW *wv = (MSIWHEREVIEW*)view;

    TRACE("%p %d %p %p\n", wv, n, name, type );

    if( !wv->table )
         return ERROR_FUNCTION_FAILED;

    return wv->table->ops->get_column_info( wv->table, n, name, type );
}

static UINT WHERE_modify( struct tagMSIVIEW *view, MSIMODIFY eModifyMode, MSIHANDLE hrec)
{
    MSIWHEREVIEW *wv = (MSIWHEREVIEW*)view;

    TRACE("%p %d %ld\n", wv, eModifyMode, hrec );

    if( !wv->table )
         return ERROR_FUNCTION_FAILED;

    return wv->table->ops->modify( wv->table, eModifyMode, hrec );
}

static UINT WHERE_delete( struct tagMSIVIEW *view )
{
    MSIWHEREVIEW *wv = (MSIWHEREVIEW*)view;

    TRACE("%p\n", wv );

    if( wv->table )
        wv->table->ops->delete( wv->table );

    if( wv->reorder )
        HeapFree( GetProcessHeap(), 0, wv->reorder );
    wv->reorder = NULL;
    wv->row_count = 0;

    delete_expr( wv->cond );

    HeapFree( GetProcessHeap(), 0, wv );

    return ERROR_SUCCESS;
}


MSIVIEWOPS where_ops =
{
    WHERE_fetch_int,
    NULL,
    NULL,
    WHERE_execute,
    WHERE_close,
    WHERE_get_dimensions,
    WHERE_get_column_info,
    WHERE_modify,
    WHERE_delete
};

UINT WHERE_CreateView( MSIDATABASE *db, MSIVIEW **view, MSIVIEW *table )
{
    MSIWHEREVIEW *wv = NULL;
    UINT count = 0, r;

    TRACE("%p\n", wv );

    r = table->ops->get_dimensions( table, NULL, &count );
    if( r != ERROR_SUCCESS )
    {
        ERR("can't get table dimensions\n");
        return r;
    }

    wv = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *wv );
    if( !wv )
        return ERROR_FUNCTION_FAILED;
    
    /* fill the structure */
    wv->view.ops = &where_ops;
    wv->db = db;
    wv->table = table;
    wv->row_count = 0;
    wv->reorder = NULL;
    *view = (MSIVIEW*) wv;

    return ERROR_SUCCESS;
}

static UINT WHERE_VerifyCondition( MSIVIEW *table, struct expr *cond,
                                   UINT *valid )
{
    UINT r, col = 0;

    switch( cond->type )
    {
    case EXPR_COLUMN:
        r = VIEW_find_column( table, cond->u.column, &col );
        if( r == ERROR_SUCCESS )
        {
            *valid = 1;
            cond->type = EXPR_COL_NUMBER;
            cond->u.col_number = col;
        }
        else
        {
            *valid = 0;
            ERR("Couldn't find column %s\n", debugstr_w( cond->u.column ) );
        }
        break;
    case EXPR_COMPLEX:
        r = WHERE_VerifyCondition( table, cond->u.expr.left, valid );
        if( r != ERROR_SUCCESS )
            return r;
        if( !*valid )
            return ERROR_SUCCESS;
        r = WHERE_VerifyCondition( table, cond->u.expr.right, valid );
        if( r != ERROR_SUCCESS )
            return r;
        break;
    case EXPR_IVAL:
        *valid = 1;
        cond->type = EXPR_UVAL;
        cond->u.uval = cond->u.ival + (1<<15);
        break;
    case EXPR_SVAL:
        *valid = 0;
        FIXME("can't deal with string values yet\n");
        break;
    default:
        ERR("Invalid expression type\n");
        *valid = 0;
        break;
    } 

    return ERROR_SUCCESS;
}

UINT WHERE_AddCondition( MSIVIEW *view, struct expr *cond )
{
    MSIWHEREVIEW *wv = (MSIWHEREVIEW *) view;
    UINT r, valid = 0;

    if( wv->view.ops != &where_ops )
        return ERROR_FUNCTION_FAILED;
    if( !wv->table )
        return ERROR_INVALID_PARAMETER;
    
    if( !cond )
        return ERROR_SUCCESS;

    TRACE("Adding condition\n");

    r = WHERE_VerifyCondition( wv->table, cond, &valid );
    if( r != ERROR_SUCCESS )
        ERR("condition evaluation failed\n");

    TRACE("condition is %s\n", valid ? "valid" : "invalid" );
    if( !valid )
    {
        delete_expr( cond );
        return ERROR_FUNCTION_FAILED;
    }

    wv->cond = cond;

    return ERROR_SUCCESS;
}
