/*
 * IDL Compiler
 *
 * Copyright 2002 Ove Kaaven
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

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <signal.h>

#include "widl.h"
#include "utils.h"
#include "parser.h"
#include "header.h"

static int indentation = 0;

static void indent(int delta)
{
  int c;
  if (delta < 0) indentation += delta;
  for (c=0; c<indentation; c++) fprintf(header, "    ");
  if (delta > 0) indentation += delta;
}

int is_attr(attr_t *a, enum attr_type t)
{
  while (a) {
    if (a->type == t) return 1;
    a = NEXT_LINK(a);
  }
  return 0;
}

void *get_attrp(attr_t *a, enum attr_type t)
{
  while (a) {
    if (a->type == t) return a->u.pval;
    a = NEXT_LINK(a);
  }
  return NULL;
}

DWORD get_attrv(attr_t *a, enum attr_type t)
{
  while (a) {
    if (a->type == t) return a->u.ival;
    a = NEXT_LINK(a);
  }
  return 0;
}

int is_void(type_t *t, var_t *v)
{
  if (v && v->ptr_level) return 0;
  if (!t->type && !t->ref) return 1;
  return 0;
}

static void write_pident(FILE *h, var_t *v)
{
  int c;
  for (c=0; c<v->ptr_level; c++) {
    fprintf(h, "*");
  }
  if (v->name) fprintf(h, "%s", v->name);
}

void write_name(FILE *h, var_t *v)
{
  fprintf(h, "%s", v->name);
}

char* get_name(var_t *v)
{
  return v->name;
}

static void write_array(FILE *h, expr_t *v, int field)
{
  if (!v) return;
  while (NEXT_LINK(v)) v = NEXT_LINK(v);
  fprintf(h, "[");
  while (v) {
    if (v->is_const)
      fprintf(h, "%ld", v->cval); /* statically sized array */
    else
      if (field) fprintf(h, "1"); /* dynamically sized array */
    if (PREV_LINK(v))
      fprintf(h, ", ");
    v = PREV_LINK(v);
  }
  fprintf(h, "]");
}

static void write_field(FILE *h, var_t *v)
{
  if (!v) return;
  if (v->type) {
    indent(0);
    write_type(h, v->type, NULL, v->tname);
    if (get_name(v)) {
      fprintf(h, " ");
      write_pident(h, v);
    }
    else {
      /* not all C/C++ compilers support anonymous structs and unions */
      switch (v->type->type) {
      case RPC_FC_STRUCT:
      case RPC_FC_ENCAPSULATED_UNION:
        fprintf(h, " DUMMYSTRUCTNAME");
        break;
      case RPC_FC_NON_ENCAPSULATED_UNION:
        fprintf(h, " DUMMYUNIONNAME");
        break;
      default:
        /* ? */
        break;
      }
    }
    write_array(h, v->array, 1);
    fprintf(h, ";\n");
  }
}

static void write_fields(FILE *h, var_t *v)
{
  var_t *first = v;
  if (!v) return;
  while (NEXT_LINK(v)) v = NEXT_LINK(v);
  while (v) {
    write_field(h, v);
    if (v == first) break;
    v = PREV_LINK(v);
  }
}

static void write_enums(FILE *h, var_t *v)
{
  if (!v) return;
  while (NEXT_LINK(v)) v = NEXT_LINK(v);
  while (v) {
    if (get_name(v)) {
      indent(0);
      write_name(h, v);
      if (v->eval) {
        fprintf(h, " = ");
        write_expr(h, v->eval);
      }
    }
    if (PREV_LINK(v))
      fprintf(h, ",\n");
    v = PREV_LINK(v);
  }
  fprintf(h, "\n");
}

void write_type(FILE *h, type_t *t, var_t *v, char *n)
{
  int c;

  if (n) fprintf(h, "%s", n);
  else {
    if (t->is_const) fprintf(h, "const ");
    if (t->type) {
      if (t->sign > 0) fprintf(h, "signed ");
      else if (t->sign < 0) fprintf(h, "unsigned ");
      switch (t->type) {
      case RPC_FC_BYTE:
        if (t->ref) fprintf(h, t->ref->name);
        else fprintf(h, "byte");
        break;
      case RPC_FC_CHAR:
        if (t->ref) fprintf(h, t->ref->name);
        else fprintf(h, "char");
        break;
      case RPC_FC_WCHAR:
        fprintf(h, "wchar_t");
        break;
      case RPC_FC_USHORT:
      case RPC_FC_SHORT:
        if (t->ref) fprintf(h, t->ref->name);
        else fprintf(h, "short");
        break;
      case RPC_FC_ULONG:
      case RPC_FC_LONG:
        if (t->ref) fprintf(h, t->ref->name);
        else fprintf(h, "long");
        break;
      case RPC_FC_HYPER:
        if (t->ref) fprintf(h, t->ref->name);
        else fprintf(h, "hyper");
        break;
      case RPC_FC_FLOAT:
        fprintf(h, "float");
        break;
      case RPC_FC_DOUBLE:
        fprintf(h, "double");
        break;
      case RPC_FC_ENUM16:
      case RPC_FC_ENUM32:
        if (t->defined && !t->written) {
          if (t->name) fprintf(h, "enum %s {\n", t->name);
          else fprintf(h, "enum {\n");
          t->written = TRUE;
          indentation++;
          write_enums(h, t->fields);
          indent(-1);
          fprintf(h, "}");
        }
        else fprintf(h, "enum %s", t->name);
        break;
      case RPC_FC_ERROR_STATUS_T:
        if (t->ref) fprintf(h, t->ref->name);
        else fprintf(h, "error_status_t");
        break;
      case RPC_FC_BIND_PRIMITIVE:
        if (t->ref) fprintf(h, t->ref->name);
        else fprintf(h, "handle_t");
        break;
      case RPC_FC_STRUCT:
      case RPC_FC_ENCAPSULATED_UNION:
        if (t->defined && !t->written) {
          if (t->name) fprintf(h, "struct %s {\n", t->name);
          else fprintf(h, "struct {\n");
          t->written = TRUE;
          indentation++;
          write_fields(h, t->fields);
          indent(-1);
          fprintf(h, "}");
        }
        else fprintf(h, "struct %s", t->name);
        break;
      case RPC_FC_NON_ENCAPSULATED_UNION:
        if (t->defined && !t->written) {
          if (t->name) fprintf(h, "union %s {\n", t->name);
          else fprintf(h, "union {\n");
          t->written = TRUE;
          indentation++;
          write_fields(h, t->fields);
          indent(-1);
          fprintf(h, "}");
        }
        else fprintf(h, "union %s", t->name);
        break;
      default:
        fprintf(h, "(unknown-type:%d)", t->type);
      }
    }
    else {
      if (t->ref) {
        write_type(h, t->ref, NULL, t->name);
      }
      else fprintf(h, "void");
    }
  }
  if (v) {
    for (c=0; c<v->ptr_level; c++) {
      fprintf(h, "*");
    }
  }
}

void write_typedef(type_t *type, var_t *names)
{
  char *tname = names->tname;
  var_t *lname;
  while (NEXT_LINK(names)) names = NEXT_LINK(names);
  lname = names;
  fprintf(header, "typedef ");
  write_type(header, type, NULL, tname);
  fprintf(header, " ");
  while (names) {
    write_pident(header, names);
    if (PREV_LINK(names))
      fprintf(header, ", ");
    names = PREV_LINK(names);
  }
  fprintf(header, ";\n");

  if (get_attrp(type->attrs, ATTR_WIREMARSHAL)) {
    names = lname;
    while (names) {
      char *name = get_name(names);
      fprintf(header, "unsigned long   __RPC_USER %s_UserSize     (unsigned long *, unsigned long,   %s *);\n", name, name);
      fprintf(header, "unsigned char * __RPC_USER %s_UserMarshal  (unsigned long *, unsigned char *, %s *);\n", name, name);
      fprintf(header, "unsigned char * __RPC_USER %s_UserUnmarshal(unsigned long *, unsigned char *, %s *);\n", name, name);
      fprintf(header, "void            __RPC_USER %s_UserFree     (unsigned long *, %s *);\n", name, name);
      if (PREV_LINK(names))
        fprintf(header, ", ");
      names = PREV_LINK(names);
    }
  }

  fprintf(header, "\n");
}

static void do_write_expr(FILE *h, expr_t *e, int p)
{
  switch (e->type) {
  case EXPR_VOID:
    break;
  case EXPR_NUM:
    fprintf(h, "%ld", e->u.lval);
    break;
  case EXPR_HEXNUM:
    fprintf(h, "0x%lx", e->u.lval);
    break;
  case EXPR_IDENTIFIER:
    fprintf(h, "%s", e->u.sval);
    break;
  case EXPR_NEG:
    fprintf(h, "-");
    do_write_expr(h, e->ref, 1);
    break;
  case EXPR_NOT:
    fprintf(h, "~");
    do_write_expr(h, e->ref, 1);
    break;
  case EXPR_PPTR:
    fprintf(h, "*");
    do_write_expr(h, e->ref, 1);
    break;
  case EXPR_CAST:
    fprintf(h, "(");
    write_type(h, e->u.tref->ref, NULL, e->u.tref->name);
    fprintf(h, ")");
    do_write_expr(h, e->ref, 1);
    break;
  case EXPR_SIZEOF:
    fprintf(h, "sizeof(");
    write_type(h, e->u.tref->ref, NULL, e->u.tref->name);
    fprintf(h, ")");
    break;
  case EXPR_SHL:
  case EXPR_SHR:
  case EXPR_MUL:
  case EXPR_DIV:
  case EXPR_ADD:
  case EXPR_SUB:
  case EXPR_AND:
  case EXPR_OR:
    if (p) fprintf(h, "(");
    do_write_expr(h, e->ref, 1);
    switch (e->type) {
    case EXPR_SHL: fprintf(h, " << "); break;
    case EXPR_SHR: fprintf(h, " >> "); break;
    case EXPR_MUL: fprintf(h, " * "); break;
    case EXPR_DIV: fprintf(h, " / "); break;
    case EXPR_ADD: fprintf(h, " + "); break;
    case EXPR_SUB: fprintf(h, " - "); break;
    case EXPR_AND: fprintf(h, " & "); break;
    case EXPR_OR:  fprintf(h, " | "); break;
    default: break;
    }
    do_write_expr(h, e->u.ext, 1);
    if (p) fprintf(h, ")");
    break;
  }
}

void write_expr(FILE *h, expr_t *e)
{
  do_write_expr(h, e, 0);
}

void write_constdef(var_t *v)
{
  fprintf(header, "#define %s (", get_name(v));
  write_expr(header, v->eval);
  fprintf(header, ")\n\n");
}

void write_externdef(var_t *v)
{
  fprintf(header, "extern const ");
  write_type(header, v->type, NULL, v->tname);
  if (get_name(v)) {
    fprintf(header, " ");
    write_pident(header, v);
  }
  fprintf(header, ";\n\n");
}

/********** INTERFACES **********/

int is_object(attr_t *a)
{
  while (a) {
    if (a->type == ATTR_OBJECT || a->type == ATTR_ODL) return 1;
    a = NEXT_LINK(a);
  }
  return 0;
}

int is_local(attr_t *a)
{
  return is_attr(a, ATTR_LOCAL);
}

var_t *is_callas(attr_t *a)
{
  return get_attrp(a, ATTR_CALLAS);
}

static void write_icom_method_def(type_t *iface)
{
  func_t *cur = iface->funcs;
  if (iface->ref) write_icom_method_def( iface->ref );
  if (!cur) return;
  while (NEXT_LINK(cur)) cur = NEXT_LINK(cur);
  if (cur) fprintf( header, " \\\n    /*** %s methods ***/", iface->name );
  while (cur) {
    var_t *def = cur->def;
    if (!is_callas(def->attrs)) {
      var_t *arg = cur->args;

      if (arg) {
	while (NEXT_LINK(arg)) {
	  arg = NEXT_LINK(arg);
	}
      }
      fprintf(header, " \\\n    STDMETHOD_(");
      write_type(header, def->type, def, def->tname);
      fprintf(header, ",");
      write_name(header, def);
      fprintf(header, ")(%s", arg ? "THIS_ " : "THIS" );
      while (arg) {
	write_type(header, arg->type, arg, arg->tname);
        if (arg->args)
        {
          fprintf(header, " (STDMETHODCALLTYPE *");
          write_name(header,arg);
          fprintf( header,")(");
          write_args(header, arg->args, NULL, 0, FALSE);
          fprintf(header,")");
        }
        else
        {
          fprintf(header, " ");
          write_name(header,arg);
        }
	write_array(header, arg->array, 0);
	arg = PREV_LINK(arg);
	if (arg) fprintf(header, ", ");
      }
      fprintf(header, ") PURE;");
    }
    cur = PREV_LINK(cur);
  }
}

static int write_method_macro(type_t *iface, char *name)
{
  int idx;
  func_t *cur = iface->funcs;

  if (iface->ref) idx = write_method_macro(iface->ref, name);
  else idx = 0;

  if (!cur) return idx;
  while (NEXT_LINK(cur)) cur = NEXT_LINK(cur);

  fprintf(header, "/*** %s methods ***/\n", iface->name);
  while (cur) {
    var_t *def = cur->def;
    if (!is_callas(def->attrs)) {
      var_t *arg = cur->args;
      int argc = 0;
      int c;
      while (arg) {
	arg = NEXT_LINK(arg);
	argc++;
      }

      fprintf(header, "#define %s_", name);
      write_name(header,def);
      fprintf(header, "(p");
      for (c=0; c<argc; c++)
	fprintf(header, ",%c", c+'a');
      fprintf(header, ") ");

      fprintf(header, "(p)->lpVtbl->");
      write_name(header, def);
      fprintf(header, "(p");
      for (c=0; c<argc; c++)
	fprintf(header, ",%c", c+'a');
      fprintf(header, ")\n");
      if (cur->idx == -1) cur->idx = idx;
      else if (cur->idx != idx) yyerror("BUG: method index mismatch in write_method_macro");
      idx++;
    }
    cur = PREV_LINK(cur);
  }
  return idx;
}

void write_args(FILE *h, var_t *arg, char *name, int method, int do_indent)
{
  int count = 0;
  if (arg) {
    while (NEXT_LINK(arg))
      arg = NEXT_LINK(arg);
  }
  if (do_indent)
  {
      if (h == header) {
          indentation++;
          indent(0);
      } else fprintf(h, "    ");
  }
  if (method == 1) {
    fprintf(h, "%s* This", name);
    count++;
  }
  while (arg) {
    if (count) {
        if (do_indent)
        {
            fprintf(h, ",\n");
            if (h == header) indent(0);
            else fprintf(h, "    ");
        }
        else fprintf(h, ",");
    }
    write_type(h, arg->type, arg, arg->tname);
    if (arg->args)
    {
      fprintf(h, " (STDMETHODCALLTYPE *");
      write_name(h,arg);
      fprintf(h, ")(");
      write_args(h, arg->args, NULL, 0, FALSE);
      fprintf(h, ")");
    }
    else
    {
      fprintf(h, " ");
      write_name(h, arg);
    }
    write_array(h, arg->array, 0);
    arg = PREV_LINK(arg);
    count++;
  }
  if (do_indent && h == header) indentation--;
}

static void write_cpp_method_def(type_t *iface)
{
  func_t *cur = iface->funcs;

  if (!cur) return;
  while (NEXT_LINK(cur)) cur = NEXT_LINK(cur);
  while (cur) {
    var_t *def = cur->def;
    if (!is_callas(def->attrs)) {
      indent(0);
      fprintf(header, "virtual ");
      write_type(header, def->type, def, def->tname);
      fprintf(header, " STDMETHODCALLTYPE ");
      write_name(header, def);
      fprintf(header, "(\n");
      write_args(header, cur->args, iface->name, 2, TRUE);
      fprintf(header, ") = 0;\n");
      fprintf(header, "\n");
    }
    cur = PREV_LINK(cur);
  }
}

static void do_write_c_method_def(type_t *iface, char *name)
{
  func_t *cur = iface->funcs;

  if (iface->ref) do_write_c_method_def(iface->ref, name);

  if (!cur) return;
  while (NEXT_LINK(cur)) cur = NEXT_LINK(cur);
  indent(0);
  fprintf(header, "/*** %s methods ***/\n", iface->name);
  while (cur) {
    var_t *def = cur->def;
    if (!is_callas(def->attrs)) {
      indent(0);
      write_type(header, def->type, def, def->tname);
      fprintf(header, " (STDMETHODCALLTYPE *");
      write_name(header, def);
      fprintf(header, ")(\n");
      write_args(header, cur->args, name, 1, TRUE);
      fprintf(header, ");\n");
      fprintf(header, "\n");
    }
    cur = PREV_LINK(cur);
  }
}

static void write_c_method_def(type_t *iface)
{
  do_write_c_method_def(iface, iface->name);
}

static void write_method_proto(type_t *iface)
{
  func_t *cur = iface->funcs;

  if (!cur) return;
  while (NEXT_LINK(cur)) cur = NEXT_LINK(cur);
  while (cur) {
    var_t *def = cur->def;
    var_t *cas = is_callas(def->attrs);
    if (!is_local(def->attrs)) {
      /* proxy prototype */
      write_type(header, def->type, def, def->tname);
      fprintf(header, " CALLBACK %s_", iface->name);
      write_name(header, def);
      fprintf(header, "_Proxy(\n");
      write_args(header, cur->args, iface->name, 1, TRUE);
      fprintf(header, ");\n");
      /* stub prototype */
      fprintf(header, "void __RPC_STUB %s_", iface->name);
      write_name(header,def);
      fprintf(header, "_Stub(\n");
      fprintf(header, "    struct IRpcStubBuffer* This,\n");
      fprintf(header, "    struct IRpcChannelBuffer* pRpcChannelBuffer,\n");
      fprintf(header, "    PRPC_MESSAGE pRpcMessage,\n");
      fprintf(header, "    DWORD* pdwStubPhase);\n");
    }
    if (cas) {
      func_t *m = iface->funcs;
      while (m && strcmp(get_name(m->def), cas->name))
        m = NEXT_LINK(m);
      if (m) {
        var_t *mdef = m->def;
        /* proxy prototype - use local prototype */
        write_type(header, mdef->type, mdef, mdef->tname);
        fprintf(header, " CALLBACK %s_", iface->name);
        write_name(header, mdef);
        fprintf(header, "_Proxy(\n");
        write_args(header, m->args, iface->name, 1, TRUE);
        fprintf(header, ");\n");
        /* stub prototype - use remotable prototype */
        write_type(header, def->type, def, def->tname);
        fprintf(header, " __RPC_STUB %s_", iface->name);
        write_name(header, mdef);
        fprintf(header, "_Stub(\n");
        write_args(header, cur->args, iface->name, 1, TRUE);
        fprintf(header, ");\n");
      }
      else {
        yywarning("invalid call_as attribute (%s -> %s)\n", get_name(def), cas->name);
      }
    }

    cur = PREV_LINK(cur);
  }
}

static void write_function_proto(type_t *iface)
{
  func_t *cur = iface->funcs;
  while (NEXT_LINK(cur)) cur = NEXT_LINK(cur);
  while (cur) {
    var_t *def = cur->def;
    /* FIXME: do we need to handle call_as? */
    write_type(header, def->type, def, def->tname);
    fprintf(header, " ");
    write_name(header, def);
    fprintf(header, "(\n");
    write_args(header, cur->args, iface->name, 0, TRUE);
    fprintf(header, ");\n");

    cur = PREV_LINK(cur);
  }
}

void write_forward(type_t *iface)
{
  /* C/C++ forwards should only be written for object interfaces, so if we
   * have a full definition we only write one if we find [object] among the
   * attributes - however, if we don't have a full definition at this point
   * (i.e. this is an IDL forward), then we also assume that it is an object
   * interface, since non-object interfaces shouldn't need forwards */
  if ((!iface->defined || is_object(iface->attrs)) && !iface->written) {
    fprintf(header,"#ifndef __%s_FWD_DEFINED__\n", iface->name);
    fprintf(header,"#define __%s_FWD_DEFINED__\n", iface->name);
    fprintf(header, "typedef struct %s %s;\n", iface->name, iface->name);
    fprintf(header, "#endif\n\n" );
    iface->written = TRUE;
  }
}

void write_guid(type_t *iface)
{
  UUID *uuid = get_attrp(iface->attrs, ATTR_UUID);
  if (!uuid) return;
  fprintf(header, "DEFINE_GUID(IID_%s, 0x%08lx, 0x%04x, 0x%04x, 0x%02x,0x%02x, 0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x);\n",
          iface->name, uuid->Data1, uuid->Data2, uuid->Data3, uuid->Data4[0], uuid->Data4[1],
          uuid->Data4[2], uuid->Data4[3], uuid->Data4[4], uuid->Data4[5], uuid->Data4[6], uuid->Data4[7]);
}

void write_com_interface(type_t *iface)
{
  if (!iface->funcs && !iface->ref) {
    yywarning("%s has no methods", iface->name);
    return;
  }

  fprintf(header, "/*****************************************************************************\n");
  fprintf(header, " * %s interface\n", iface->name);
  fprintf(header, " */\n");
  fprintf(header,"#ifndef __%s_INTERFACE_DEFINED__\n", iface->name);
  fprintf(header,"#define __%s_INTERFACE_DEFINED__\n\n", iface->name);
  write_guid(iface);
  write_forward(iface);
  /* C++ interface */
  fprintf(header, "#if defined(__cplusplus) && !defined(CINTERFACE)\n");
  if (iface->ref)
      fprintf(header, "struct %s : public %s\n", iface->name, iface->ref->name);
  else
  {
      fprintf(header, "#ifdef ICOM_USE_COM_INTERFACE_ATTRIBUTE\n");
      fprintf(header, "struct __attribute__((com_interface)) %s\n", iface->name);
      fprintf(header, "#else\n");
      fprintf(header, "struct %s\n", iface->name);
      fprintf(header, "#endif\n");
  }
  fprintf(header, "{\n");
  indentation++;
  write_cpp_method_def(iface);
  indentation--;
  fprintf(header, "};\n");
  fprintf(header, "#else\n");
  /* C interface */
  fprintf(header, "typedef struct %sVtbl %sVtbl;\n", iface->name, iface->name);
  fprintf(header, "struct %s {\n", iface->name);
  fprintf(header, "    const %sVtbl* lpVtbl;\n", iface->name);
  fprintf(header, "};\n");
  fprintf(header, "struct %sVtbl {\n", iface->name);
  indentation++;
  fprintf(header, "    ICOM_MSVTABLE_COMPAT_FIELDS\n");
  fprintf(header, "\n");
  write_c_method_def(iface);
  indentation--;
  fprintf(header, "};\n");
  fprintf(header, "\n");
  write_method_macro(iface, iface->name);
  fprintf(header, "\n");
  fprintf(header, "#endif\n");
  fprintf(header, "\n");
  if (compat_icom) {
      fprintf(header, "#define %s_METHODS \\\n", iface->name);
      fprintf(header, "    ICOM_MSVTABLE_COMPAT_FIELDS");
      write_icom_method_def(iface);
      fprintf(header, "\n\n");
  }
  write_method_proto(iface);
  fprintf(header,"\n#endif  /* __%s_INTERFACE_DEFINED__ */\n\n", iface->name);
}

void write_rpc_interface(type_t *iface)
{
  DWORD ver = get_attrv(iface->attrs, ATTR_VERSION);

  if (!iface->funcs) return;

  fprintf(header, "/*****************************************************************************\n");
  fprintf(header, " * %s interface (v%d.%d)\n", iface->name, LOWORD(ver), HIWORD(ver));
  fprintf(header, " */\n");
  write_guid(iface);
  fprintf(header, "extern RPC_IF_HANDLE %s_v%d_%d_c_ifspec;\n", iface->name, LOWORD(ver), HIWORD(ver));
  fprintf(header, "extern RPC_IF_HANDLE %s_v%d_%d_s_ifspec;\n", iface->name, LOWORD(ver), HIWORD(ver));
  write_function_proto(iface);
  fprintf(header, "\n");

  /* FIXME: server/client code */
}

void write_interface(type_t *iface)
{
  if (is_object(iface->attrs))
    write_com_interface(iface);
  else
    write_rpc_interface(iface);
}
