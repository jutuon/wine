/*
 * WINElib-Resources
 *
 * Copied and modified heavily from loader/resource.c
 */

#include <stdlib.h>
#include "libres.h"
#include "resource.h"
#include "debug.h"
#include "heap.h"
#include "windows.h"
#include "xmalloc.h"

typedef struct RLE
{
    const wrc_resource32_t * const * Resources;  /* NULL-terminated array of pointers */
    struct RLE* next;
} ResListE;

static ResListE* ResourceList=NULL;

void LIBRES_RegisterResources(const wrc_resource32_t * const * Res)
{
  ResListE** Curr;
  ResListE* n;
  for(Curr=&ResourceList; *Curr; Curr=&((*Curr)->next)) { }
  n=xmalloc(sizeof(ResListE));
  n->Resources=Res;
  n->next=NULL;
  *Curr=n;
}

/**********************************************************************
 *	    LIBRES_FindResource
 */
HRSRC32 LIBRES_FindResource( HINSTANCE32 hModule, LPCWSTR name, LPCWSTR type )
{
  int nameid=0,typeid;
  ResListE* ResBlock;
  const wrc_resource32_t* const * Res;

  if(HIWORD(name))
  {
    if(*name=='#')
    {
        LPSTR nameA = HEAP_strdupWtoA( GetProcessHeap(), 0, name );
        nameid = atoi(nameA+1);
        HeapFree( GetProcessHeap(), 0, nameA );
        name=NULL;
    }
  }
  else
  {
    nameid=LOWORD(name);
    name=NULL;
  }
  if(HIWORD(type))
  {
    if(*type=='#')
    {
        LPSTR typeA = HEAP_strdupWtoA( GetProcessHeap(), 0, type );
        typeid=atoi(typeA+1);
        HeapFree( GetProcessHeap(), 0, typeA );
    }
    else
    {
      TRACE(resource, "(*,*,type=string): Returning 0\n");
      return 0;
    }
  }
  else
    typeid=LOWORD(type);
  
  /* FIXME: types can be strings */
  for(ResBlock=ResourceList; ResBlock; ResBlock=ResBlock->next)
    for(Res=ResBlock->Resources; *Res; Res++)
      if(name)
      {
	if((*Res)->restype==typeid && !lstrncmpi32W((LPCWSTR)((*Res)->resname+1), name, *((*Res)->resname)))
	  return (HRSRC32)*Res;
      }
      else
	if((*Res)->restype==typeid && (*Res)->resid==nameid)
	  return (HRSRC32)*Res;
  return 0;
}


/**********************************************************************
 *	    LIBRES_LoadResource    
 */
HGLOBAL32 LIBRES_LoadResource( HINSTANCE32 hModule, HRSRC32 hRsrc )
{
  return (HGLOBAL32)(((wrc_resource32_t*)hRsrc)->data);
}


/**********************************************************************
 *	    LIBRES_SizeofResource    
 */
DWORD LIBRES_SizeofResource( HINSTANCE32 hModule, HRSRC32 hRsrc )
{
  return (DWORD)(((wrc_resource32_t*)hRsrc)->datasize);
}
