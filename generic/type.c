#include <tcl.h>
#include <string.h>
#include "tclstuff.h"
#include "tip445.h"
#if DEBUG
#	include "names.h"
#endif

#ifdef __builtin_expect
#	define likely(exp)   __builtin_expect(!!(exp), 1)
#	define unlikely(exp) __builtin_expect(!!(exp), 0)
#else
#	define likely(exp)   (exp)
#	define unlikely(exp) (exp)
#endif

#ifdef BUILD_objtype
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT
#endif /* BUILD_objtype */

struct objtype_ex {
	Tcl_ObjType	base;
	Tcl_Interp*	interp;
	Tcl_Obj*	typename;
	Tcl_Obj*	update_string_rep;
	Tcl_Obj*	free_int_rep;
	Tcl_Obj*	dup_int_rep;

	/*
	 * Tcl list to which the value and optional args from [type get] will
	 * be appended and executed to build the intrep Tcl_Obj
	 */

	Tcl_Obj*	create;
};

enum {
	LIT_CREATE,
	LIT_UPDATE_STRING_REP,
	LIT_FREE_INT_REP,
	LIT_DUP_INT_REP,
	LIT_END
};

static const char* lit_strings[] = {
	"create",
	"string",
	"free",
	"dup",
	NULL
};

struct pidata {
	Tcl_HashTable	types;
	Tcl_Obj*		lit[LIT_END];
};

static void free_int_rep(Tcl_Obj* obj) //{{{
{
	/*
	 * Violates TIP 445, at least in spirit.  Need a way to iterate through all
	 * obj's types to find those that we manage, in case there are more than
	 * one (Hydra)
	 */
	struct objtype_ex*	type = (struct objtype_ex*)obj->typePtr;
	Tcl_ObjIntRep*	ir = Tcl_FetchIntRep(obj, (Tcl_ObjType*)type);
	Tcl_Obj*		intrep = ir->twoPtrValue.ptr1;
	Tcl_InterpState	state;

	if (intrep) {
		state = Tcl_SaveInterpState(type->interp, 0);
		if (type->free_int_rep) {
			Tcl_Obj*	cmd = NULL;
			int			code = TCL_OK;

			replace_tclobj(&cmd, Tcl_DuplicateObj(type->free_int_rep));
			code = Tcl_ListObjAppendElement(type->interp, cmd, intrep);
			if (code == TCL_OK)
				code = Tcl_EvalObjEx(type->interp, cmd, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
			release_tclobj(&cmd);
			if (code != TCL_OK)
				Tcl_Panic("type(%s) free handler \"%s\" threw error: %s",
						Tcl_GetString(type->typename),
						Tcl_GetString(type->free_int_rep),
						Tcl_GetString(Tcl_GetObjResult(type->interp)));
		}
		release_tclobj(&intrep);
		ir->twoPtrValue.ptr1 = NULL;
		Tcl_RestoreInterpState(type->interp, state);
	}
}

//}}}
static void dup_int_rep(Tcl_Obj* src, Tcl_Obj* dup) //{{{
{
	/*
	 * Violates TIP 445, at least in spirit.  Need a way to iterate through all
	 * obj's types to find those that we manage, in case there are more than
	 * one (Hydra)
	 */
	struct objtype_ex*	type = (struct objtype_ex*)src->typePtr;
	Tcl_ObjIntRep*	ir = Tcl_FetchIntRep(src, (Tcl_ObjType*)type);
	Tcl_Obj*		intrep = NULL;
	Tcl_ObjIntRep	newIr;

	memset(&newIr, 0, sizeof(newIr));

	replace_tclobj(&intrep, ir->twoPtrValue.ptr1);

	if (intrep) {
		if (type->dup_int_rep) {
			Tcl_Obj*	cmd = NULL;
			int			code = TCL_OK;
			Tcl_InterpState	state;

			state = Tcl_SaveInterpState(type->interp, 0);
			replace_tclobj(&cmd, Tcl_DuplicateObj(type->dup_int_rep));
			code = Tcl_ListObjAppendElement(type->interp, cmd, intrep);
			if (code == TCL_OK)
				code = Tcl_EvalObjEx(type->interp, cmd, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
			release_tclobj(&cmd);
			if (code != TCL_OK)
				Tcl_Panic("type(%s) dup handler \"%s\" threw error: %s",
						Tcl_GetString(type->typename),
						Tcl_GetString(type->dup_int_rep),
						Tcl_GetString(Tcl_GetObjResult(type->interp)));

			if (code == TCL_OK) {
				replace_tclobj(&intrep, Tcl_GetObjResult(type->interp));
			} else {
				// TODO: what?
			}
			Tcl_RestoreInterpState(type->interp, state);
		}
	}
	replace_tclobj((Tcl_Obj**)&newIr.twoPtrValue.ptr1, intrep);

	Tcl_StoreIntRep(dup, (Tcl_ObjType*)type, &newIr);
}

//}}}
static void update_string_rep(Tcl_Obj* obj) //{{{
{
	/*
	 * Violates TIP 445, at least in spirit.  Need a way to iterate through all
	 * obj's types to find those that we manage, in case there are more than
	 * one (Hydra)
	 */
	struct objtype_ex*	type = (struct objtype_ex*)obj->typePtr;
	Tcl_ObjIntRep*	ir = Tcl_FetchIntRep(obj, (Tcl_ObjType*)type);
	Tcl_Obj*		intrep = NULL;

	replace_tclobj(&intrep, ir->twoPtrValue.ptr1);

	if (intrep) {
		if (type->update_string_rep) {
			Tcl_Obj*		cmd = NULL;
			int				code = TCL_OK;
			Tcl_InterpState	state = Tcl_SaveInterpState(type->interp, 0);

			replace_tclobj(&cmd, Tcl_DuplicateObj(type->update_string_rep));
			code = Tcl_ListObjAppendElement(type->interp, cmd, intrep);
			if (code == TCL_OK)
				code = Tcl_EvalObjEx(type->interp, cmd, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
			release_tclobj(&cmd);
			if (code != TCL_OK)
				Tcl_Panic("type(%s) string handler \"%s\" threw error: %s",
						Tcl_GetString(type->typename),
						Tcl_GetString(type->update_string_rep),
						Tcl_GetString(Tcl_GetObjResult(type->interp)));

			{
				Tcl_Obj*	res = Tcl_GetObjResult(type->interp);
				const char*	newstring = NULL;
				int			newstring_len;

				newstring = Tcl_GetStringFromObj(res, &newstring_len);

				Tcl_InitStringRep(obj, newstring, newstring_len);
			}
			Tcl_RestoreInterpState(type->interp, state);
		} else {
			// TODO: what?
			Tcl_Panic("regobjtype(%s) update_string_rep not defined", Tcl_GetString(type->typename));
		}
	} else {
		// TODO: what?
		Tcl_Panic("regobjtype(%s) intrep Tcl_Obj is NULL", Tcl_GetString(type->typename));
	}
}

//}}}
static void free_handlers(struct objtype_ex* handlers) //{{{
{
	if (handlers->base.name) {
		ckfree(handlers->base.name);
		handlers->base.name = NULL;
	}

	release_tclobj(&handlers->typename);
	release_tclobj(&handlers->update_string_rep);
	release_tclobj(&handlers->free_int_rep);
	release_tclobj(&handlers->dup_int_rep);
	release_tclobj(&handlers->create);

	ckfree(handlers);
}

//}}}
static int define(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	struct pidata*		l = cdata;
	int					code = TCL_OK;
	int					new;
	Tcl_HashEntry*		he = NULL;
	struct objtype_ex*	handlers = NULL;
	Tcl_Obj*			typename = NULL;
	const char*			typename_str = NULL;
	int					typename_len;
	static const char*	handler_names[] = {
		/*
		 * Tempting to use lit_strings for this, but it isn't guaranteed
		 * to match this list, and will almost certainly become a superset
		 * in the future.
		 */
		"create",
		"string",
		"free",
		"dup",
		NULL
	};
	enum {
		HANDLER_CREATE,
		HANDLER_UPDATE_STRING_REP,
		HANDLER_FREE_INT_REP,
		HANDLER_DUP_INT_REP
	};

	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "type handlers");
		code = TCL_ERROR;
		goto finally;
	}

	he = Tcl_CreateHashEntry(&l->types, objv[1], &new);
	if (unlikely(he == NULL)) {
		// Not sure this is possible
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Failed to create a hash entry for \"%s\"", Tcl_GetString(objv[1])));
		code = TCL_ERROR;
		goto finally;
	}

	if (likely(new)) {
		handlers = ckalloc(sizeof(*handlers));
		memset(handlers, 0, sizeof(*handlers));
		replace_tclobj(&typename, Tcl_ObjPrintf("objtype_%s", Tcl_GetString(objv[1])));
		replace_tclobj(&handlers->typename, typename);

		typename_str = Tcl_GetStringFromObj(typename, &typename_len);
		handlers->base.name = ckalloc(typename_len+1);
		memcpy((char*)handlers->base.name, typename_str, typename_len+1);

		handlers->interp = interp;
		handlers->base.freeIntRepProc	= free_int_rep;
		handlers->base.dupIntRepProc	= dup_int_rep;
		handlers->base.updateStringProc	= update_string_rep;
		handlers->base.setFromAnyProc	= NULL;		// We don't Tcl_RegisterObjType this

		Tcl_SetHashValue(he, handlers);
	} else {
		// Redefine an existing type
		handlers = Tcl_GetHashValue(he);
	}

	/*
	 * Iterate through handlers and copy the commands to our extended
	 * Tcl_ObjType.  If any invalid keys are defined an error is thrown
	 */
	{
		Tcl_DictSearch	search;
		Tcl_Obj*		k = NULL;
		Tcl_Obj*		v = NULL;
		int				done;

		code = Tcl_DictObjFirst(interp, objv[2], &search, &k, &v, &done);
		for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
			int handler_index;

			code = Tcl_GetIndexFromObj(interp, k, handler_names, "handler", TCL_EXACT, &handler_index);
			if (code != TCL_OK) goto donesearch;

/*
 * When we call these handlers later, if they throw an exception we have no
 * option but to Tcl_Panic, so check what we can at define time: at least that
 * the command is a valid list (so that Tcl_ListObjAppendElement is unlikely to
 * fail), that it has at least one element, and that we can look up the command
 * for the first element of the list in the global namespace.
 */
#define CHECK_HANDLER_IS_LIST(obj) \
		do { \
			int oc; \
			Tcl_Obj** ov; \
			Tcl_Command cmd; \
			code = Tcl_ListObjGetElements(interp, obj, &oc, &ov); \
			if (code != TCL_OK) goto donesearch; \
			if (oc == 0) { \
				Tcl_SetObjResult(interp, Tcl_ObjPrintf( \
							"Handler %s of \"%s\" cannot be an empty list", \
							handler_names[handler_index], \
							Tcl_GetString(objv[1]))); \
				code = TCL_ERROR; \
				goto donesearch; \
			} \
			cmd = Tcl_FindCommand(interp, Tcl_GetString(ov[0]), \
					Tcl_GetGlobalNamespace(interp), \
					TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG); \
			if (cmd == NULL) { \
				code = TCL_ERROR; \
				goto donesearch; \
			} \
		} while(0);

			switch (handler_index) {
				case HANDLER_CREATE:
					CHECK_HANDLER_IS_LIST(v);
					replace_tclobj(&handlers->create, v);
					break;
				case HANDLER_FREE_INT_REP:
					CHECK_HANDLER_IS_LIST(v);
					replace_tclobj(&handlers->free_int_rep, v);
					break;
				case HANDLER_DUP_INT_REP:
					CHECK_HANDLER_IS_LIST(v);
					replace_tclobj(&handlers->dup_int_rep, v);
					break;
				case HANDLER_UPDATE_STRING_REP:
					CHECK_HANDLER_IS_LIST(v);
					replace_tclobj(&handlers->update_string_rep, v);
					break;
				default:
					Tcl_SetObjResult(interp, Tcl_ObjPrintf("Invalid handler_index: %d", handler_index));
					code = TCL_ERROR;
					goto donesearch;
			}
		}
donesearch:
		Tcl_DictObjDone(&search);
		if (code != TCL_OK) goto finally;

		if (handlers->create == NULL) {
			Tcl_SetObjResult(interp, Tcl_ObjPrintf("create is required in handlers"));
			handlers = NULL;
			code = TCL_ERROR;
			goto finally;
		}
	}

	he = NULL;
	handlers = NULL;	// Pass ownership of the memory to the hash entry

finally:
	if (likely(typename)) release_tclobj(&typename);

	if (he) {
		Tcl_DeleteHashEntry(he);
		he = NULL;
	}

	if (unlikely(handlers)) {
		free_handlers(handlers);
		handlers = NULL;
	}

	return code;
}

//}}}
static int fetch_or_create_intrep(Tcl_Interp* interp, struct pidata* l, Tcl_Obj* val, Tcl_Obj* type, Tcl_ObjIntRep** irout) //{{{
{
	int					code = TCL_OK;
	Tcl_ObjIntRep*		ir = NULL;
	Tcl_HashEntry*		he = NULL;
	struct objtype_ex*	handlers;
	Tcl_ObjType*		basetype = NULL;
	Tcl_Obj*			cmd = NULL;
	Tcl_Obj**			tv = NULL;
	int					tc;

	code = Tcl_ListObjGetElements(interp, type, &tc, &tv);
	if (code != TCL_OK) goto finally;

	if (tc < 1 || tc > 2) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
					"type must be a list of one or two elements: "
					"{typename ?context?}, got %d elements: %s",
					tc, Tcl_GetString(type)));
		code = TCL_ERROR;
		goto finally;
	}

	he = Tcl_FindHashEntry(&l->types, tv[0]);
	if (he == NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Type \"%s\" is not defined", Tcl_GetString(tv[0])));
		code = TCL_ERROR;
		goto finally;
	}

	handlers = Tcl_GetHashValue(he);
	basetype = &handlers->base;

	ir = Tcl_FetchIntRep(val, basetype);
	if (ir == NULL || (tc > 1 && ir->twoPtrValue.ptr2 != tv[1])) {
		Tcl_InterpState	state = NULL;

		Tcl_ObjIntRep newIr = {.twoPtrValue.ptr1 = NULL, .twoPtrValue.ptr2 = NULL};

		replace_tclobj(&cmd, Tcl_DuplicateObj(handlers->create));
		code = Tcl_ListObjAppendElement(interp, cmd, val);
		if (code == TCL_ERROR) goto finally;
		if (tc > 1) {
			code = Tcl_ListObjAppendElement(interp, cmd, tv[1]);
			if (code == TCL_ERROR) goto finally;
		}
		if (handlers->interp != interp) {
			state = Tcl_SaveInterpState(handlers->interp, 0);
		}
		code = Tcl_EvalObjEx(handlers->interp, cmd, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
		release_tclobj(&cmd);
		if (code == TCL_OK) {
			replace_tclobj((Tcl_Obj**)&newIr.twoPtrValue.ptr1, Tcl_GetObjResult(handlers->interp));
			if (tc > 1) replace_tclobj((Tcl_Obj**)&newIr.twoPtrValue.ptr2, tv[1]);
			Tcl_StoreIntRep(val, (Tcl_ObjType*)handlers, &newIr);
			ir = Tcl_FetchIntRep(val, basetype);
		}
		if (handlers->interp != interp) {
			Tcl_SetObjResult(interp, Tcl_GetObjResult(handlers->interp));
			// TODO: migrate the rest of the result state (maybe Tcl_SaveInterpState(handlers->interp), Tcl_RestoreInterpState(interp) ?
			Tcl_RestoreInterpState(handlers->interp, state);
		}
	}

	if (code == TCL_OK) {
		if (ir == NULL) {
			/*
			* Shouldn't be possible - it means that we couldn't retrieve the
			* intrep we just stored
			*/
			Tcl_SetObjResult(interp, Tcl_ObjPrintf(
						"Could not retrive type(%s) intrep from %s",
						Tcl_GetString(tv[0]), Tcl_GetString(val)));
			code = TCL_ERROR;
			goto finally;
		} else {
			*irout = ir;
		}
	}

finally:
	release_tclobj(&cmd);

	return code;
}

//}}}
static int get(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	Tcl_ObjIntRep*		ir = NULL;
	struct pidata*		l = cdata;

	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "type value");
		return TCL_ERROR;
	}

	if (fetch_or_create_intrep(interp, l, objv[2], objv[1], &ir) == TCL_OK) {
		Tcl_SetObjResult(interp, ir->twoPtrValue.ptr1);
		return TCL_OK;
	} else {
		return TCL_ERROR;
	}
}

//}}}
static int with(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	int					code = TCL_OK;
	Tcl_ObjIntRep*		ir = NULL;
	struct pidata*		l = cdata;
	Tcl_Obj*			val = NULL;
	Tcl_Obj*			loanval = NULL;
	Tcl_InterpState		saved_state;

	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "varname type script");
		code = TCL_ERROR;
		goto finally;
	}

	loanval = Tcl_ObjGetVar2(interp, objv[1], NULL, TCL_LEAVE_ERR_MSG);
	if (loanval == NULL) {
		code = TCL_ERROR;
		goto finally;
	}
	DBG("loanval refcount: %d\n", loanval->refCount);

	/* TODO: break the interface abstraction here to handle errors thrown by
	 * the dup callback more gracefully? */
	if (Tcl_IsShared(loanval))
		loanval = Tcl_DuplicateObj(loanval);

	/*
	 * Upgrade our loaned reference from the variable to a reference owned by
	 * val, since we're about to replace the variable and we need to hold on to
	 * the value.
	 */
	replace_tclobj(&val, loanval);
	loanval = NULL;

	if (fetch_or_create_intrep(interp, l, val, objv[2], &ir) != TCL_OK) {
		code = TCL_ERROR;
		goto finally;
	}

	Tcl_InvalidateStringRep(val);

	if (NULL == Tcl_ObjSetVar2(interp, objv[1], NULL, (Tcl_Obj*)ir->twoPtrValue.ptr1, TCL_LEAVE_ERR_MSG)) {
		code = TCL_ERROR;
		goto finally;
	}

	code = Tcl_EvalObjEx(interp, objv[3], 0);
	saved_state = Tcl_SaveInterpState(interp, code);

	if (NULL == Tcl_ObjSetVar2(interp, objv[1], NULL, val, TCL_LEAVE_ERR_MSG)) {
		if (code == TCL_ERROR) {
			/* This second error occured while unwinding from an original
			 * error, report that rather */
			code = Tcl_RestoreInterpState(interp, saved_state);
		} else {
			Tcl_DiscardInterpState(saved_state);
			code = TCL_ERROR;
		}
		goto finally;
	}

	code = Tcl_RestoreInterpState(interp, saved_state);

finally:
	release_tclobj(&val);

	return code;
}

//}}}
static void free_pidata(ClientData cdata, Tcl_Interp* interp) //{{{
{
	struct pidata*	l = cdata;
	int				i;
	Tcl_HashSearch	search;
	Tcl_HashEntry*	he = NULL;

	if (l) {
		he = Tcl_FirstHashEntry(&l->types, &search);
		for (; he; he=Tcl_NextHashEntry(&search)) {
			struct objtype_ex*	handlers = Tcl_GetHashValue(he);
			if (handlers) {
				free_handlers(handlers);
				handlers = NULL;
			}
			Tcl_DeleteHashEntry(he);
		}
		Tcl_DeleteHashTable(&l->types);
		for (i=0; i<LIT_END; i++) release_tclobj(&l->lit[i]);
		ckfree(l);
		l = NULL;
	}
}

//}}}

#define NS	"::type"

struct cmd {
	char*			name;
	Tcl_ObjCmdProc*	proc;
} cmds[] = {
	{NS "::define",	define},
	{NS "::get",	get},
	{NS "::with",	with},
	{NULL,			NULL}
};


#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */
DLLEXPORT int Type_Init(Tcl_Interp* interp) //{{{
{
	Tcl_Namespace*	ns = NULL;
	struct pidata*	l = NULL;
	int				code = TCL_OK;
	int				i;

#ifdef USE_TCL_STUBS
	if (Tcl_InitStubs(interp, "8.6", 0) == NULL)
		return TCL_ERROR;
#endif // USE_TCL_STUBS

	l = ckalloc(sizeof(*l));
	memset(l, 0, sizeof(*l));
	/*
	 * As of Tcl8.7a3 at least, no special magic is done for hash tables
	 * whose keys are Tcl_Objs, although I could certainly imagine caching
	 * the hash in the intrep of the key.  The hash used is so fast though
	 * it may not save much, unless the values are long.  Might also need
	 * Hydra so that it doesn't stomp on a more useful intrep of the obj.
	 * With some care, the intrep of the obj key could point straight to
	 * the value, which would definitely be a win.
	 * tldr; Use Tcl_InitObjHashTable here even though it's not a win at
	 * present, in the hope that someday it will be.
	 */
	Tcl_InitObjHashTable(&l->types);

	for (i=0; i<LIT_END; i++)
		replace_tclobj(&l->lit[i], Tcl_NewStringObj(lit_strings[i], -1));

	Tcl_SetAssocData(interp, "type", free_pidata, l);

	ns = Tcl_CreateNamespace(interp, NS, NULL, NULL);
	code = Tcl_Export(interp, ns, "*", 0);
	if (code != TCL_OK) goto finally;

	{
		struct cmd*	c = cmds;

		Tcl_CreateEnsemble(interp, NS, ns, 0);

		while (c->name != NULL) {
			if (NULL == Tcl_CreateObjCommand(interp, c->name, c->proc, l, NULL)) {
				Tcl_SetObjResult(interp, Tcl_ObjPrintf("Could not create command %s", c->name));
				code = TCL_ERROR;
				goto finally;
			}
			c++;
		}
	}

	code = Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION);
	if (code != TCL_OK) goto finally;

finally:
	return code;
}

//}}}
DLLEXPORT int Type_SafeInit(Tcl_Interp* interp) //{{{
{
	// No unsafe features
	return Type_Init(interp);
}

//}}}
DLLEXPORT int Type_Unload(Tcl_Interp* interp, int flags) //{{{
{
	Tcl_Namespace*		ns;

	switch (flags) {
		case TCL_UNLOAD_DETACH_FROM_INTERPRETER:
			ns = Tcl_FindNamespace(interp, NS, NULL, TCL_GLOBAL_ONLY);
			if (ns) {
				Tcl_DeleteNamespace(ns);
				ns = NULL;
			}
			break;
		case TCL_UNLOAD_DETACH_FROM_PROCESS:
			ns = Tcl_FindNamespace(interp, NS, NULL, TCL_GLOBAL_ONLY);
			if (ns) {
				Tcl_DeleteNamespace(ns);
				ns = NULL;
			}
			break;
		default:
			THROW_ERROR("Unhandled flags");
	}

	return TCL_OK;
}

//}}}
DLLEXPORT int Type_SafeUnload(Tcl_Interp* interp, int flags) //{{{
{
	// No unsafe features
	return Type_Unload(interp, flags);
}

//}}}

#ifdef __cplusplus
}
#endif  /* __cplusplus */

/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* End: */
// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
