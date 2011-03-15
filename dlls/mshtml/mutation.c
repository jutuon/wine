/*
 * Copyright 2008 Jacek Caban for CodeWeavers
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winreg.h"
#include "ole2.h"
#include "shlguid.h"

#include "mshtml_private.h"
#include "htmlevent.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

#define IE_MAJOR_VERSION 7
#define IE_MINOR_VERSION 0

static const IID NS_ICONTENTUTILS_CID =
    {0x762C4AE7,0xB923,0x422F,{0xB9,0x7E,0xB9,0xBF,0xC1,0xEF,0x7B,0xF0}};

static nsIContentUtils *content_utils;

static BOOL handle_insert_comment(HTMLDocumentNode *doc, const PRUnichar *comment)
{
    DWORD len;
    int majorv = 0, minorv = 0;
    const PRUnichar *ptr, *end;
    nsAString nsstr;
    PRUnichar *buf;
    nsresult nsres;

    enum {
        CMP_EQ,
        CMP_LT,
        CMP_LTE,
        CMP_GT,
        CMP_GTE
    } cmpt = CMP_EQ;

    static const PRUnichar endifW[] = {'<','!','[','e','n','d','i','f',']'};

    if(comment[0] != '[' || comment[1] != 'i' || comment[2] != 'f')
        return FALSE;

    ptr = comment+3;
    while(isspaceW(*ptr))
        ptr++;

    if(ptr[0] == 'l' && ptr[1] == 't') {
        ptr += 2;
        if(*ptr == 'e') {
            cmpt = CMP_LTE;
            ptr++;
        }else {
            cmpt = CMP_LT;
        }
    }else if(ptr[0] == 'g' && ptr[1] == 't') {
        ptr += 2;
        if(*ptr == 'e') {
            cmpt = CMP_GTE;
            ptr++;
        }else {
            cmpt = CMP_GT;
        }
    }

    if(!isspaceW(*ptr++))
        return FALSE;
    while(isspaceW(*ptr))
        ptr++;

    if(ptr[0] != 'I' || ptr[1] != 'E')
        return FALSE;

    ptr +=2;
    if(!isspaceW(*ptr++))
        return FALSE;
    while(isspaceW(*ptr))
        ptr++;

    if(!isdigitW(*ptr))
        return FALSE;
    while(isdigitW(*ptr))
        majorv = majorv*10 + (*ptr++ - '0');

    if(*ptr == '.') {
        ptr++;
        if(!isdigitW(*ptr))
            return FALSE;
        while(isdigitW(*ptr))
            minorv = minorv*10 + (*ptr++ - '0');
    }

    while(isspaceW(*ptr))
        ptr++;
    if(ptr[0] != ']' || ptr[1] != '>')
        return FALSE;
    ptr += 2;

    len = strlenW(ptr);
    if(len < sizeof(endifW)/sizeof(WCHAR))
        return FALSE;

    end = ptr + len-sizeof(endifW)/sizeof(WCHAR);
    if(memcmp(end, endifW, sizeof(endifW)))
        return FALSE;

    switch(cmpt) {
    case CMP_EQ:
        if(majorv == IE_MAJOR_VERSION && minorv == IE_MINOR_VERSION)
            break;
        return FALSE;
    case CMP_LT:
        if(majorv > IE_MAJOR_VERSION)
            break;
        if(majorv == IE_MAJOR_VERSION && minorv > IE_MINOR_VERSION)
            break;
        return FALSE;
    case CMP_LTE:
        if(majorv > IE_MAJOR_VERSION)
            break;
        if(majorv == IE_MAJOR_VERSION && minorv >= IE_MINOR_VERSION)
            break;
        return FALSE;
    case CMP_GT:
        if(majorv < IE_MAJOR_VERSION)
            break;
        if(majorv == IE_MAJOR_VERSION && minorv < IE_MINOR_VERSION)
            break;
        return FALSE;
    case CMP_GTE:
        if(majorv < IE_MAJOR_VERSION)
            break;
        if(majorv == IE_MAJOR_VERSION && minorv <= IE_MINOR_VERSION)
            break;
        return FALSE;
    }

    buf = heap_alloc((end-ptr+1)*sizeof(WCHAR));
    if(!buf)
        return FALSE;

    memcpy(buf, ptr, (end-ptr)*sizeof(WCHAR));
    buf[end-ptr] = 0;
    nsAString_InitDepend(&nsstr, buf);

    /* FIXME: Find better way to insert HTML to document. */
    nsres = nsIDOMHTMLDocument_Write(doc->nsdoc, &nsstr);
    nsAString_Finish(&nsstr);
    heap_free(buf);
    if(NS_FAILED(nsres)) {
        ERR("Write failed: %08x\n", nsres);
        return FALSE;
    }

    return TRUE;
}

static nsresult run_insert_comment(HTMLDocumentNode *doc, nsISupports *comment_iface, nsISupports *arg2)
{
    const PRUnichar *comment;
    nsIDOMComment *nscomment;
    nsAString comment_str;
    BOOL remove_comment;
    nsresult nsres;

    nsres = nsISupports_QueryInterface(comment_iface, &IID_nsIDOMComment, (void**)&nscomment);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIDOMComment iface:%08x\n", nsres);
        return nsres;
    }

    nsAString_Init(&comment_str, NULL);
    nsres = nsIDOMComment_GetData(nscomment, &comment_str);
    if(NS_FAILED(nsres))
        return nsres;

    nsAString_GetData(&comment_str, &comment);
    remove_comment = handle_insert_comment(doc, comment);
    nsAString_Finish(&comment_str);

    if(remove_comment) {
        nsIDOMNode *nsparent, *tmp;
        nsAString magic_str;

        static const PRUnichar remove_comment_magicW[] =
            {'#','!','w','i','n','e', 'r','e','m','o','v','e','!','#',0};

        nsAString_InitDepend(&magic_str, remove_comment_magicW);
        nsres = nsIDOMComment_SetData(nscomment, &magic_str);
        nsAString_Finish(&magic_str);
        if(NS_FAILED(nsres))
            ERR("SetData failed: %08x\n", nsres);

        nsIDOMComment_GetParentNode(nscomment, &nsparent);
        if(nsparent) {
            nsIDOMNode_RemoveChild(nsparent, (nsIDOMNode*)nscomment, &tmp);
            nsIDOMNode_Release(nsparent);
            nsIDOMNode_Release(tmp);
        }
    }

    nsIDOMComment_Release(nscomment);
    return NS_OK;
}

static nsresult run_bind_to_tree(HTMLDocumentNode *doc, nsISupports *nsiface, nsISupports *arg2)
{
    nsIDOMNode *nsnode;
    HTMLDOMNode *node;
    nsresult nsres;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", doc, nsiface);

    nsres = nsISupports_QueryInterface(nsiface, &IID_nsIDOMNode, (void**)&nsnode);
    if(NS_FAILED(nsres))
        return nsres;

    hres = get_node(doc, nsnode, TRUE, &node);
    nsIDOMNode_Release(nsnode);
    if(FAILED(hres)) {
        ERR("Could not get node\n");
        return nsres;
    }

    if(node->vtbl->bind_to_tree)
        node->vtbl->bind_to_tree(node);

    return nsres;
}

/* Calls undocumented 69 cmd of CGID_Explorer */
static void call_explorer_69(HTMLDocumentObj *doc)
{
    IOleCommandTarget *olecmd;
    VARIANT var;
    HRESULT hres;

    if(!doc->client)
        return;

    hres = IOleClientSite_QueryInterface(doc->client, &IID_IOleCommandTarget, (void**)&olecmd);
    if(FAILED(hres))
        return;

    VariantInit(&var);
    hres = IOleCommandTarget_Exec(olecmd, &CGID_Explorer, 69, 0, NULL, &var);
    IOleCommandTarget_Release(olecmd);
    if(SUCCEEDED(hres) && V_VT(&var) != VT_NULL)
        FIXME("handle result\n");
}

static void parse_complete(HTMLDocumentObj *doc)
{
    TRACE("(%p)\n", doc);

    if(doc->usermode == EDITMODE)
        init_editor(&doc->basedoc);

    call_explorer_69(doc);
    if(doc->view_sink)
        IAdviseSink_OnViewChange(doc->view_sink, DVASPECT_CONTENT, -1);
    call_property_onchanged(&doc->basedoc.cp_propnotif, 1005);
    call_explorer_69(doc);

    /* FIXME: IE7 calls EnableModelless(TRUE), EnableModelless(FALSE) and sets interactive state here */
}

static nsresult run_end_load(HTMLDocumentNode *This, nsISupports *arg1, nsISupports *arg2)
{
    TRACE("(%p)\n", This);

    if(!This->basedoc.doc_obj)
        return NS_OK;

    if(This == This->basedoc.doc_obj->basedoc.doc_node) {
        /*
         * This should be done in the worker thread that parses HTML,
         * but we don't have such thread (Gecko parses HTML for us).
         */
        parse_complete(This->basedoc.doc_obj);
    }

    set_ready_state(This->basedoc.window, READYSTATE_INTERACTIVE);
    return NS_OK;
}

static nsresult run_insert_script(HTMLDocumentNode *doc, nsISupports *script_iface, nsISupports *arg)
{
    nsIDOMHTMLScriptElement *nsscript;
    nsresult nsres;

    TRACE("(%p)->(%p)\n", doc, script_iface);

    nsres = nsISupports_QueryInterface(script_iface, &IID_nsIDOMHTMLScriptElement, (void**)&nsscript);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIDOMHTMLScriptElement: %08x\n", nsres);
        return nsres;
    }

    doc_insert_script(doc->basedoc.window, nsscript);
    nsIDOMHTMLScriptElement_Release(nsscript);
    return NS_OK;
}

typedef struct nsRunnable nsRunnable;

typedef nsresult (*runnable_proc_t)(HTMLDocumentNode*,nsISupports*,nsISupports*);

struct nsRunnable {
    nsIRunnable  nsIRunnable_iface;

    LONG ref;

    runnable_proc_t proc;

    HTMLDocumentNode *doc;
    nsISupports *arg1;
    nsISupports *arg2;
};

static inline nsRunnable *impl_from_nsIRunnable(nsIRunnable *iface)
{
    return CONTAINING_RECORD(iface, nsRunnable, nsIRunnable_iface);
}

static nsresult NSAPI nsRunnable_QueryInterface(nsIRunnable *iface,
        nsIIDRef riid, void **result)
{
    nsRunnable *This = impl_from_nsIRunnable(iface);

    if(IsEqualGUID(riid, &IID_nsISupports)) {
        TRACE("(%p)->(IID_nsISupports %p)\n", This, result);
        *result = &This->nsIRunnable_iface;
    }else if(IsEqualGUID(riid, &IID_nsIRunnable)) {
        TRACE("(%p)->(IID_nsIRunnable %p)\n", This, result);
        *result = &This->nsIRunnable_iface;
    }else {
        *result = NULL;
        WARN("(%p)->(%s %p)\n", This, debugstr_guid(riid), result);
        return NS_NOINTERFACE;
    }

    nsISupports_AddRef((nsISupports*)*result);
    return NS_OK;
}

static nsrefcnt NSAPI nsRunnable_AddRef(nsIRunnable *iface)
{
    nsRunnable *This = impl_from_nsIRunnable(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static nsrefcnt NSAPI nsRunnable_Release(nsIRunnable *iface)
{
    nsRunnable *This = impl_from_nsIRunnable(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        htmldoc_release(&This->doc->basedoc);
        if(This->arg1)
            nsISupports_Release(This->arg1);
        if(This->arg2)
            nsISupports_Release(This->arg2);
        heap_free(This);
    }

    return ref;
}

static nsresult NSAPI nsRunnable_Run(nsIRunnable *iface)
{
    nsRunnable *This = impl_from_nsIRunnable(iface);

    return This->proc(This->doc, This->arg1, This->arg2);
}

static const nsIRunnableVtbl nsRunnableVtbl = {
    nsRunnable_QueryInterface,
    nsRunnable_AddRef,
    nsRunnable_Release,
    nsRunnable_Run
};

static void add_script_runner(HTMLDocumentNode *This, runnable_proc_t proc, nsISupports *arg1, nsISupports *arg2)
{
    nsRunnable *runnable;

    runnable = heap_alloc_zero(sizeof(*runnable));
    if(!runnable)
        return;

    runnable->nsIRunnable_iface.lpVtbl = &nsRunnableVtbl;
    runnable->ref = 1;

    htmldoc_addref(&This->basedoc);
    runnable->doc = This;
    runnable->proc = proc;

    if(arg1)
        nsISupports_AddRef(arg1);
    runnable->arg1 = arg1;

    if(arg2)
        nsISupports_AddRef(arg2);
    runnable->arg2 = arg2;

    nsIContentUtils_AddScriptRunner(content_utils, &runnable->nsIRunnable_iface);

    nsIRunnable_Release(&runnable->nsIRunnable_iface);
}

static inline HTMLDocumentNode *impl_from_nsIDocumentObserver(nsIDocumentObserver *iface)
{
    return CONTAINING_RECORD(iface, HTMLDocumentNode, nsIDocumentObserver_iface);
}

static nsresult NSAPI nsDocumentObserver_QueryInterface(nsIDocumentObserver *iface,
        nsIIDRef riid, void **result)
{
    HTMLDocumentNode *This = impl_from_nsIDocumentObserver(iface);

    if(IsEqualGUID(&IID_nsISupports, riid)) {
        TRACE("(%p)->(IID_nsISupports, %p)\n", This, result);
        *result = &This->nsIDocumentObserver_iface;
    }else if(IsEqualGUID(&IID_nsIMutationObserver, riid)) {
        TRACE("(%p)->(IID_nsIMutationObserver %p)\n", This, result);
        *result = &This->nsIDocumentObserver_iface;
    }else if(IsEqualGUID(&IID_nsIDocumentObserver, riid)) {
        TRACE("(%p)->(IID_nsIDocumentObserver %p)\n", This, result);
        *result = &This->nsIDocumentObserver_iface;
    }else {
        *result = NULL;
        TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), result);
        return NS_NOINTERFACE;
    }

    htmldoc_addref(&This->basedoc);
    return NS_OK;
}

static nsrefcnt NSAPI nsDocumentObserver_AddRef(nsIDocumentObserver *iface)
{
    HTMLDocumentNode *This = impl_from_nsIDocumentObserver(iface);
    return htmldoc_addref(&This->basedoc);
}

static nsrefcnt NSAPI nsDocumentObserver_Release(nsIDocumentObserver *iface)
{
    HTMLDocumentNode *This = impl_from_nsIDocumentObserver(iface);
    return htmldoc_release(&This->basedoc);
}

static void NSAPI nsDocumentObserver_CharacterDataWillChange(nsIDocumentObserver *iface,
        nsIDocument *aDocument, nsIContent *aContent, void /*CharacterDataChangeInfo*/ *aInfo)
{
}

static void NSAPI nsDocumentObserver_CharacterDataChanged(nsIDocumentObserver *iface,
        nsIDocument *aDocument, nsIContent *aContent, void /*CharacterDataChangeInfo*/ *aInfo)
{
}

static void NSAPI nsDocumentObserver_AttributeWillChange(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIContent *aContent, PRInt32 aNameSpaceID, nsIAtom *aAttribute, PRInt32 aModType)
{
}

static void NSAPI nsDocumentObserver_AttributeChanged(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIContent *aContent, PRInt32 aNameSpaceID, nsIAtom *aAttribute, PRInt32 aModType)
{
}

static void NSAPI nsDocumentObserver_ContentAppended(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIContent *aContainer, nsIContent *aFirstNewContent, PRInt32 aNewIndexInContainer)
{
}

static void NSAPI nsDocumentObserver_ContentInserted(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIContent *aContainer, nsIContent *aChild, PRInt32 aIndexInContainer)
{
}

static void NSAPI nsDocumentObserver_ContentRemoved(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIContent *aContainer, nsIContent *aChild, PRInt32 aIndexInContainer,
        nsIContent *aProviousSibling)
{
}

static void NSAPI nsDocumentObserver_NodeWillBeDestroyed(nsIDocumentObserver *iface, const nsINode *aNode)
{
}

static void NSAPI nsDocumentObserver_ParentChainChanged(nsIDocumentObserver *iface, nsIContent *aContent)
{
}

static void NSAPI nsDocumentObserver_BeginUpdate(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsUpdateType aUpdateType)
{
}

static void NSAPI nsDocumentObserver_EndUpdate(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsUpdateType aUpdateType)
{
}

static void NSAPI nsDocumentObserver_BeginLoad(nsIDocumentObserver *iface, nsIDocument *aDocument)
{
}

static void NSAPI nsDocumentObserver_EndLoad(nsIDocumentObserver *iface, nsIDocument *aDocument)
{
    HTMLDocumentNode *This = impl_from_nsIDocumentObserver(iface);

    TRACE("(%p)\n", This);

    if(This->skip_mutation_notif)
        return;

    This->content_ready = TRUE;
    add_script_runner(This, run_end_load, NULL, NULL);
}

static void NSAPI nsDocumentObserver_ContentStatesChanged(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIContent *aContent1, nsIContent *aContent2, nsEventStates aStateMask)
{
}

static void NSAPI nsDocumentObserver_DocumentStatesChanged(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsEventStates aStateMask)
{
}

static void NSAPI nsDocumentObserver_StyleSheetAdded(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIStyleSheet *aStyleSheet, PRBool aDocumentSheet)
{
}

static void NSAPI nsDocumentObserver_StyleSheetRemoved(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIStyleSheet *aStyleSheet, PRBool aDocumentSheet)
{
}

static void NSAPI nsDocumentObserver_StyleSheetApplicableStateChanged(nsIDocumentObserver *iface,
        nsIDocument *aDocument, nsIStyleSheet *aStyleSheet, PRBool aApplicable)
{
}

static void NSAPI nsDocumentObserver_StyleRuleChanged(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIStyleSheet *aStyleSheet, nsIStyleRule *aOldStyleRule, nsIStyleSheet *aNewStyleRule)
{
}

static void NSAPI nsDocumentObserver_StyleRuleAdded(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIStyleSheet *aStyleSheet, nsIStyleRule *aStyleRule)
{
}

static void NSAPI nsDocumentObserver_StyleRuleRemoved(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIStyleSheet *aStyleSheet, nsIStyleRule *aStyleRule)
{
}

static void NSAPI nsDocumentObserver_BindToDocument(nsIDocumentObserver *iface, nsIDocument *aDocument,
        nsIContent *aContent)
{
    HTMLDocumentNode *This = impl_from_nsIDocumentObserver(iface);
    nsIDOMHTMLIFrameElement *nsiframe;
    nsIDOMHTMLFrameElement *nsframe;
    nsIDOMComment *nscomment;
    nsIDOMElement *nselem;
    nsresult nsres;

    TRACE("(%p)\n", This);

    nsres = nsISupports_QueryInterface(aContent, &IID_nsIDOMElement, (void**)&nselem);
    if(NS_SUCCEEDED(nsres)) {
        check_event_attr(This, nselem);
        nsIDOMElement_Release(nselem);
    }

    nsres = nsISupports_QueryInterface(aContent, &IID_nsIDOMComment, (void**)&nscomment);
    if(NS_SUCCEEDED(nsres)) {
        TRACE("comment node\n");

        add_script_runner(This, run_insert_comment, (nsISupports*)nscomment, NULL);
        nsIDOMComment_Release(nscomment);
    }

    nsres = nsISupports_QueryInterface(aContent, &IID_nsIDOMHTMLIFrameElement, (void**)&nsiframe);
    if(NS_SUCCEEDED(nsres)) {
        TRACE("iframe node\n");

        add_script_runner(This, run_bind_to_tree, (nsISupports*)nsiframe, NULL);
        nsIDOMHTMLIFrameElement_Release(nsiframe);
    }

    nsres = nsISupports_QueryInterface(aContent, &IID_nsIDOMHTMLFrameElement, (void**)&nsframe);
    if(NS_SUCCEEDED(nsres)) {
        TRACE("frame node\n");

        add_script_runner(This, run_bind_to_tree, (nsISupports*)nsframe, NULL);
        nsIDOMHTMLFrameElement_Release(nsframe);
    }
}

static nsresult NSAPI nsDocumentObserver_DoneAddingChildren(nsIDocumentObserver *iface, nsIContent *aContent,
        PRBool aHaveNotified, nsIParser *aParser)
{
    HTMLDocumentNode *This = impl_from_nsIDocumentObserver(iface);
    nsIDOMHTMLScriptElement *nsscript;
    nsresult nsres;

    TRACE("(%p)->(%p %x)\n", This, aContent, aHaveNotified);

    nsres = nsISupports_QueryInterface(aContent, &IID_nsIDOMHTMLScriptElement, (void**)&nsscript);
    if(NS_SUCCEEDED(nsres)) {
        TRACE("script node\n");

        add_script_runner(This, run_insert_script, (nsISupports*)nsscript, NULL);
        nsIDOMHTMLScriptElement_Release(nsscript);
    }

    return NS_OK;
}

static const nsIDocumentObserverVtbl nsDocumentObserverVtbl = {
    nsDocumentObserver_QueryInterface,
    nsDocumentObserver_AddRef,
    nsDocumentObserver_Release,
    nsDocumentObserver_CharacterDataWillChange,
    nsDocumentObserver_CharacterDataChanged,
    nsDocumentObserver_AttributeWillChange,
    nsDocumentObserver_AttributeChanged,
    nsDocumentObserver_ContentAppended,
    nsDocumentObserver_ContentInserted,
    nsDocumentObserver_ContentRemoved,
    nsDocumentObserver_NodeWillBeDestroyed,
    nsDocumentObserver_ParentChainChanged,
    nsDocumentObserver_BeginUpdate,
    nsDocumentObserver_EndUpdate,
    nsDocumentObserver_BeginLoad,
    nsDocumentObserver_EndLoad,
    nsDocumentObserver_ContentStatesChanged,
    nsDocumentObserver_DocumentStatesChanged,
    nsDocumentObserver_StyleSheetAdded,
    nsDocumentObserver_StyleSheetRemoved,
    nsDocumentObserver_StyleSheetApplicableStateChanged,
    nsDocumentObserver_StyleRuleChanged,
    nsDocumentObserver_StyleRuleAdded,
    nsDocumentObserver_StyleRuleRemoved,
    nsDocumentObserver_BindToDocument,
    nsDocumentObserver_DoneAddingChildren
};

void init_document_mutation(HTMLDocumentNode *doc)
{
    nsIDocument *nsdoc;
    nsresult nsres;

    doc->nsIDocumentObserver_iface.lpVtbl = &nsDocumentObserverVtbl;

    nsres = nsIDOMHTMLDocument_QueryInterface(doc->nsdoc, &IID_nsIDocument, (void**)&nsdoc);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIDocument: %08x\n", nsres);
        return;
    }

    nsIContentUtils_AddDocumentObserver(content_utils, nsdoc, &doc->nsIDocumentObserver_iface);
    nsIDocument_Release(nsdoc);
}

void release_document_mutation(HTMLDocumentNode *doc)
{
    nsIDocument *nsdoc;
    nsresult nsres;

    nsres = nsIDOMHTMLDocument_QueryInterface(doc->nsdoc, &IID_nsIDocument, (void**)&nsdoc);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIDocument: %08x\n", nsres);
        return;
    }

    nsIContentUtils_RemoveDocumentObserver(content_utils, nsdoc, &doc->nsIDocumentObserver_iface);
    nsIDocument_Release(nsdoc);
}

void init_mutation(nsIComponentManager *component_manager)
{
    nsIFactory *factory;
    nsresult nsres;

    if(!component_manager) {
        if(content_utils) {
            nsIContentUtils_Release(content_utils);
            content_utils = NULL;
        }
        return;
    }

    nsres = nsIComponentManager_GetClassObject(component_manager, &NS_ICONTENTUTILS_CID,
            &IID_nsIFactory, (void**)&factory);
    if(NS_FAILED(nsres)) {
        ERR("Could not create nsIContentUtils service: %08x\n", nsres);
        return;
    }

    nsres = nsIFactory_CreateInstance(factory, NULL, &IID_nsIContentUtils, (void**)&content_utils);
    nsIFactory_Release(factory);
    if(NS_FAILED(nsres))
        ERR("Could not create nsIContentUtils instance: %08x\n", nsres);
}
