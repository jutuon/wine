/*
 * GDI brush objects
 *
 * Copyright 1993, 1994  Alexandre Julliard
 */

#include <stdlib.h>
#include "winbase.h"
#include "brush.h"
#include "bitmap.h"
#include "debug.h"


/***********************************************************************
 *           CreateBrushIndirect16    (GDI.50)
 */
HBRUSH16 WINAPI CreateBrushIndirect16( const LOGBRUSH16 * brush )
{
    BRUSHOBJ * brushPtr;
    HBRUSH16 hbrush = GDI_AllocObject( sizeof(BRUSHOBJ), BRUSH_MAGIC );
    if (!hbrush) return 0;
    brushPtr = (BRUSHOBJ *) GDI_HEAP_LOCK( hbrush );
    brushPtr->logbrush.lbStyle = brush->lbStyle;
    brushPtr->logbrush.lbColor = brush->lbColor;
    brushPtr->logbrush.lbHatch = brush->lbHatch;
    GDI_HEAP_UNLOCK( hbrush );
    TRACE(gdi, "%04x\n", hbrush);
    return hbrush;
}


/***********************************************************************
 *           CreateBrushIndirect32    (GDI32.27)
 */
HBRUSH32 WINAPI CreateBrushIndirect32( const LOGBRUSH32 * brush )
{
    BRUSHOBJ * brushPtr;
    HBRUSH32 hbrush = GDI_AllocObject( sizeof(BRUSHOBJ), BRUSH_MAGIC );
    if (!hbrush) return 0;
    brushPtr = (BRUSHOBJ *) GDI_HEAP_LOCK( hbrush );
    brushPtr->logbrush.lbStyle = brush->lbStyle;
    brushPtr->logbrush.lbColor = brush->lbColor;
    brushPtr->logbrush.lbHatch = brush->lbHatch;
    GDI_HEAP_UNLOCK( hbrush );
    TRACE(gdi, "%08x\n", hbrush);
    return hbrush;
}


/***********************************************************************
 *           CreateHatchBrush16    (GDI.58)
 */
HBRUSH16 WINAPI CreateHatchBrush16( INT16 style, COLORREF color )
{
    LOGBRUSH32 logbrush = { BS_HATCHED, color, style };
    TRACE(gdi, "%d %06lx\n", style, color );
    if ((style < 0) || (style >= NB_HATCH_STYLES)) return 0;
    return CreateBrushIndirect32( &logbrush );
}


/***********************************************************************
 *           CreateHatchBrush32    (GDI32.48)
 */
HBRUSH32 WINAPI CreateHatchBrush32( INT32 style, COLORREF color )
{
    LOGBRUSH32 logbrush = { BS_HATCHED, color, style };
    TRACE(gdi, "%d %06lx\n", style, color );
    if ((style < 0) || (style >= NB_HATCH_STYLES)) return 0;
    return CreateBrushIndirect32( &logbrush );
}


/***********************************************************************
 *           CreatePatternBrush16    (GDI.60)
 */
HBRUSH16 WINAPI CreatePatternBrush16( HBITMAP16 hbitmap )
{
    return (HBRUSH16)CreatePatternBrush32( hbitmap );
}


/***********************************************************************
 *           CreatePatternBrush32    (GDI32.54)
 */
HBRUSH32 WINAPI CreatePatternBrush32( HBITMAP32 hbitmap )
{
    LOGBRUSH32 logbrush = { BS_PATTERN, 0, 0 };
    TRACE(gdi, "%04x\n", hbitmap );

    logbrush.lbHatch = (INT32)BITMAP_CopyBitmap( hbitmap );
    if(!logbrush.lbHatch)
        return 0;
    else
        return CreateBrushIndirect32( &logbrush );
}


/***********************************************************************
 *           CreateDIBPatternBrush16    (GDI.445)
 */
HBRUSH16 WINAPI CreateDIBPatternBrush16( HGLOBAL16 hbitmap, UINT16 coloruse )
{
    LOGBRUSH32 logbrush = { BS_DIBPATTERN, coloruse, 0 };
    BITMAPINFO *info, *newInfo;
    INT32 size;
    
    TRACE(gdi, "%04x\n", hbitmap );

      /* Make a copy of the bitmap */

    if (!(info = (BITMAPINFO *)GlobalLock16( hbitmap ))) return 0;

    if (info->bmiHeader.biCompression)
        size = info->bmiHeader.biSizeImage;
    else
	size = DIB_GetDIBWidthBytes(info->bmiHeader.biWidth,
				    info->bmiHeader.biBitCount) *
	       info->bmiHeader.biHeight;
    size += DIB_BitmapInfoSize( info, coloruse );

    if (!(logbrush.lbHatch = (INT16)GlobalAlloc16( GMEM_MOVEABLE, size )))
    {
	GlobalUnlock16( hbitmap );
	return 0;
    }
    newInfo = (BITMAPINFO *) GlobalLock16( (HGLOBAL16)logbrush.lbHatch );
    memcpy( newInfo, info, size );
    GlobalUnlock16( (HGLOBAL16)logbrush.lbHatch );
    GlobalUnlock16( hbitmap );
    return CreateBrushIndirect32( &logbrush );
}


/***********************************************************************
 *           CreateDIBPatternBrush32    (GDI32.34)
 *
 *	Create a logical brush which has the pattern specified by the DIB
 *
 *	Function call is for compatability only.  CreateDIBPatternBrushPt should be used.   
 *
 * RETURNS
 *
 *	Handle to a logical brush on success, NULL on failure.
 *
 * BUGS
 *	
 */
HBRUSH32 WINAPI CreateDIBPatternBrush32( 
		HGLOBAL32 hbitmap, 		/* Global object containg BITMAPINFO structure */
		UINT32 coloruse 		/* Specifies color format, if provided */
)
{
    LOGBRUSH32 logbrush = { BS_DIBPATTERN, coloruse, 0 };
    BITMAPINFO *info, *newInfo;
    INT32 size;
    
    TRACE(gdi, "%04x\n", hbitmap );

      /* Make a copy of the bitmap */

    if (!(info = (BITMAPINFO *)GlobalLock32( hbitmap ))) return 0;

    if (info->bmiHeader.biCompression)
        size = info->bmiHeader.biSizeImage;
    else
	size = DIB_GetDIBWidthBytes(info->bmiHeader.biWidth,
				    info->bmiHeader.biBitCount) *
	       info->bmiHeader.biHeight;
    size += DIB_BitmapInfoSize( info, coloruse );

    if (!(logbrush.lbHatch = (INT32)GlobalAlloc16( GMEM_MOVEABLE, size )))
    {
	GlobalUnlock16( hbitmap );
	return 0;
    }
    newInfo = (BITMAPINFO *) GlobalLock16( (HGLOBAL16)logbrush.lbHatch );
    memcpy( newInfo, info, size );
    GlobalUnlock16( (HGLOBAL16)logbrush.lbHatch );
    GlobalUnlock32( hbitmap );
    return CreateBrushIndirect32( &logbrush );
}


/***********************************************************************
 *           CreateDIBPatternBrushPt    (GDI32.35)
 *
 *	Create a logical brush which has the pattern specified by the DIB
 *
 * RETURNS
 *
 *	Handle to a logical brush on success, NULL on failure.
 *
 * BUGS
 *	
 */
HBRUSH32 WINAPI CreateDIBPatternBrushPt(
		const void* data,		/* Pointer to a BITMAPINFO structure followed by more data */ 
		UINT32 coloruse 		/* Specifies color format, if provided */
)
{
    BITMAPINFO *info=(BITMAPINFO*)data;
    LOGBRUSH32 logbrush = { BS_DIBPATTERN, coloruse, 0 };
    BITMAPINFO  *newInfo;
    INT32 size;
    
    TRACE(gdi, "%p\n", info );

      /* Make a copy of the bitmap */


    if (info->bmiHeader.biCompression)
        size = info->bmiHeader.biSizeImage;
    else
	size = DIB_GetDIBWidthBytes(info->bmiHeader.biWidth,
				    info->bmiHeader.biBitCount) *
	       info->bmiHeader.biHeight;
    size += DIB_BitmapInfoSize( info, coloruse );

    if (!(logbrush.lbHatch = (INT32)GlobalAlloc16( GMEM_MOVEABLE, size )))
    {
	return 0;
    }
    newInfo = (BITMAPINFO *) GlobalLock16( (HGLOBAL16)logbrush.lbHatch );
    memcpy( newInfo, info, size );
    GlobalUnlock16( (HGLOBAL16)logbrush.lbHatch );
    return CreateBrushIndirect32( &logbrush );
}


/***********************************************************************
 *           CreateSolidBrush    (GDI.66)
 */
HBRUSH16 WINAPI CreateSolidBrush16( COLORREF color )
{
    LOGBRUSH32 logbrush = { BS_SOLID, color, 0 };
    TRACE(gdi, "%06lx\n", color );
    return CreateBrushIndirect32( &logbrush );
}


/***********************************************************************
 *           CreateSolidBrush32    (GDI32.64)
 */
HBRUSH32 WINAPI CreateSolidBrush32( COLORREF color )
{
    LOGBRUSH32 logbrush = { BS_SOLID, color, 0 };
    TRACE(gdi, "%06lx\n", color );
    return CreateBrushIndirect32( &logbrush );
}


/***********************************************************************
 *           SetBrushOrg    (GDI.148)
 */
DWORD WINAPI SetBrushOrg( HDC16 hdc, INT16 x, INT16 y )
{
    DWORD retval;
    DC * dc = (DC *) GDI_GetObjPtr( hdc, DC_MAGIC );
    if (!dc) return FALSE;
    retval = dc->w.brushOrgX | (dc->w.brushOrgY << 16);
    dc->w.brushOrgX = x;
    dc->w.brushOrgY = y;
    return retval;
}


/***********************************************************************
 *           SetBrushOrgEx    (GDI32.308)
 */
BOOL32 WINAPI SetBrushOrgEx( HDC32 hdc, INT32 x, INT32 y, LPPOINT32 oldorg )
{
    DC * dc = (DC *) GDI_GetObjPtr( hdc, DC_MAGIC );

    if (!dc) return FALSE;
    if (oldorg)
    {
        oldorg->x = dc->w.brushOrgX;
        oldorg->y = dc->w.brushOrgY;
    }
    dc->w.brushOrgX = x;
    dc->w.brushOrgY = y;
    return TRUE;
}

/***********************************************************************
 *           FixBrushOrgEx    (GDI32.102)
 * SDK says discontinued, but in Win95 GDI32 this is the same as SetBrushOrgEx
 */
BOOL32 WINAPI FixBrushOrgEx( HDC32 hdc, INT32 x, INT32 y, LPPOINT32 oldorg )
{
    return SetBrushOrgEx(hdc,x,y,oldorg);
}


/***********************************************************************
 *           BRUSH_DeleteObject
 */
BOOL32 BRUSH_DeleteObject( HBRUSH16 hbrush, BRUSHOBJ * brush )
{
    switch(brush->logbrush.lbStyle)
    {
      case BS_PATTERN:
	  DeleteObject32( (HGDIOBJ32)brush->logbrush.lbHatch );
	  break;
      case BS_DIBPATTERN:
	  GlobalFree16( (HGLOBAL16)brush->logbrush.lbHatch );
	  break;
    }
    return GDI_FreeObject( hbrush );
}


/***********************************************************************
 *           BRUSH_GetObject16
 */
INT16 BRUSH_GetObject16( BRUSHOBJ * brush, INT16 count, LPSTR buffer )
{
    LOGBRUSH16 logbrush;

    logbrush.lbStyle = brush->logbrush.lbStyle;
    logbrush.lbColor = brush->logbrush.lbColor;
    logbrush.lbHatch = brush->logbrush.lbHatch;
    if (count > sizeof(logbrush)) count = sizeof(logbrush);
    memcpy( buffer, &logbrush, count );
    return count;
}


/***********************************************************************
 *           BRUSH_GetObject32
 */
INT32 BRUSH_GetObject32( BRUSHOBJ * brush, INT32 count, LPSTR buffer )
{
    if (count > sizeof(brush->logbrush)) count = sizeof(brush->logbrush);
    memcpy( buffer, &brush->logbrush, count );
    return count;
}


/***********************************************************************
 *           SetSolidBrush16   (GDI.604)
 *
 *  If hBrush is a solid brush, change it's color to newColor.
 *
 *  RETURNS
 *           TRUE on success, FALSE on failure.
 *   FIXME: not yet implemented!
 */
BOOL16 WINAPI SetSolidBrush16(HBRUSH16 hBrush, COLORREF newColor )
{
     FIXME(gdi, "(hBrush %04x, newColor %04x): stub!\n", hBrush, (int)newColor);

     return(FALSE);
}

