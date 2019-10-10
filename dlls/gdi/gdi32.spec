# ordinal exports
100 stdcall @(long long str str str) GDI_CallDevInstall16
101 stdcall @(long str str ptr) GDI_CallExtDeviceModePropSheet16
102 stdcall @(long ptr str str ptr str long) GDI_CallExtDeviceMode16
103 stdcall @(long str ptr ptr) GDI_CallAdvancedSetupDialog16
104 stdcall @(str str long ptr ptr) GDI_CallDeviceCapabilities16

@ stdcall AbortDoc(long)
@ stdcall AbortPath(long)
@ stdcall AddFontResourceA(str)
@ stdcall AddFontResourceExA(str long ptr)
@ stdcall AddFontResourceExW(wstr long ptr)
@ stub AddFontResourceTracking
@ stdcall AddFontResourceW(wstr)
@ stdcall AngleArc(long long long long long long)
@ stdcall AnimatePalette(long long long ptr)
@ stdcall Arc(long long long long long long long long long)
@ stdcall ArcTo(long long long long long long long long long)
@ stdcall BeginPath(long)
@ stdcall BitBlt(long long long long long long long long long)
@ stub ByeByeGDI
@ stub CancelDC
@ stub CheckColorsInGamut
@ stdcall ChoosePixelFormat(long ptr)
@ stdcall Chord(long long long long long long long long long)
@ stdcall CloseEnhMetaFile(long)
@ stdcall CloseFigure(long)
@ stdcall CloseMetaFile(long)
@ stub ColorCorrectPalette
@ stub ColorMatchToTarget
@ stdcall CombineRgn(long long long long)
@ stdcall CombineTransform(ptr ptr ptr)
@ stdcall CopyEnhMetaFileA(long str)
@ stdcall CopyEnhMetaFileW(long wstr)
@ stdcall CopyMetaFileA(long str)
@ stdcall CopyMetaFileW(long wstr)
@ stdcall CreateBitmap(long long long long ptr)
@ stdcall CreateBitmapIndirect(ptr)
@ stdcall CreateBrushIndirect(ptr)
@ stdcall CreateColorSpaceA(ptr)
@ stdcall CreateColorSpaceW(ptr)
@ stdcall CreateCompatibleBitmap(long long long)
@ stdcall CreateCompatibleDC(long)
@ stdcall CreateDCA(str str str ptr)
@ stdcall CreateDCW(wstr wstr wstr ptr)
@ stdcall CreateDIBPatternBrush(long long)
@ stdcall CreateDIBPatternBrushPt(long long)
@ stdcall CreateDIBSection(long ptr long ptr long long)
@ stdcall CreateDIBitmap(long ptr long ptr ptr long)
@ stdcall CreateDiscardableBitmap(long long long)
@ stdcall CreateEllipticRgn(long long long long)
@ stdcall CreateEllipticRgnIndirect(ptr)
@ stdcall CreateEnhMetaFileA(long str ptr str)
@ stdcall CreateEnhMetaFileW(long wstr ptr wstr)
@ stdcall CreateFontA(long long long long long long long long long long long long long str)
@ stdcall CreateFontIndirectA(ptr)
@ stdcall CreateFontIndirectW(ptr)
@ stdcall CreateFontW(long long long long long long long long long long long long long wstr)
@ stdcall CreateHalftonePalette(long)
@ stdcall CreateHatchBrush(long long)
@ stdcall CreateICA(str str str ptr)
@ stdcall CreateICW(wstr wstr wstr ptr)
@ stdcall CreateMetaFileA(str)
@ stdcall CreateMetaFileW(wstr)
@ stdcall CreatePalette(ptr)
@ stdcall CreatePatternBrush(long)
@ stdcall CreatePen(long long long)
@ stdcall CreatePenIndirect(ptr)
@ stdcall CreatePolyPolygonRgn(ptr ptr long long)
@ stdcall CreatePolygonRgn(ptr long long)
@ stdcall CreateRectRgn(long long long long)
@ stdcall CreateRectRgnIndirect(ptr)
@ stdcall CreateRoundRectRgn(long long long long long long)
@ stdcall CreateScalableFontResourceA(long str str str)
@ stdcall CreateScalableFontResourceW(long wstr wstr wstr)
@ stdcall CreateSolidBrush(long)
@ stdcall DPtoLP(long ptr long)
@ stdcall DeleteColorSpace(long)
@ stdcall DeleteDC(long)
@ stdcall DeleteEnhMetaFile(long)
@ stdcall DeleteMetaFile(long)
@ stdcall DeleteObject(long)
@ stdcall DescribePixelFormat(long long long ptr)
@ stub DeviceCapabilitiesEx
@ stub DeviceCapabilitiesExA
@ stub DeviceCapabilitiesExW
@ stdcall DrawEscape(long long long ptr)
@ stdcall Ellipse(long long long long long)
@ stub EnableEUDC
@ stdcall EndDoc(long)
@ stdcall EndPage(long)
@ stdcall EndPath(long)
@ stdcall EnumEnhMetaFile(long long ptr ptr ptr)
@ stdcall EnumFontFamiliesA(long str ptr long)
@ stdcall EnumFontFamiliesExA(long ptr ptr long long)
@ stdcall EnumFontFamiliesExW(long ptr ptr long long)
@ stdcall EnumFontFamiliesW(long wstr ptr long)
@ stdcall EnumFontsA(long str ptr long)
@ stdcall EnumFontsW(long wstr ptr long)
@ stdcall EnumICMProfilesA(long ptr long)
@ stdcall EnumICMProfilesW(long ptr long)
@ stdcall EnumMetaFile(long long ptr ptr)
@ stdcall EnumObjects(long long ptr long)
@ stdcall EqualRgn(long long)
@ stdcall Escape(long long long ptr ptr)
@ stdcall ExcludeClipRect(long long long long long)
@ stdcall ExtCreatePen(long long ptr long ptr)
@ stdcall ExtCreateRegion(ptr long ptr)
@ stdcall ExtEscape(long long long ptr long ptr)
@ stdcall ExtFloodFill(long long long long long)
@ stdcall ExtSelectClipRgn(long long long)
@ stdcall ExtTextOutA(long long long long ptr str long ptr)
@ stdcall ExtTextOutW(long long long long ptr wstr long ptr)
@ stdcall FillPath(long)
@ stdcall FillRgn(long long long)
@ stdcall FixBrushOrgEx(long long long ptr)
@ stdcall FlattenPath(long)
@ stdcall FloodFill(long long long long)
@ stdcall FrameRgn(long long long long long)
@ stub FreeImageColorMatcher
@ stub GdiAssociateObject
@ stub GdiCleanCacheDC
@ stdcall GdiComment(long long ptr)
@ stub GdiConvertAndCheckDC
@ stub GdiConvertBitmap
@ stub GdiConvertBrush
@ stub GdiConvertDC
@ stub GdiConvertEnhMetaFile
@ stub GdiConvertFont
@ stub GdiConvertMetaFilePict
@ stub GdiConvertPalette
@ stub GdiConvertRegion
@ stdcall GdiConvertToDevmodeW(ptr)
@ stub GdiCreateLocalBitmap
@ stub GdiCreateLocalBrush
@ stub GdiCreateLocalEnhMetaFile
@ stub GdiCreateLocalFont
@ stub GdiCreateLocalMetaFilePict
@ stub GdiCreateLocalPalette
@ stub GdiCreateLocalRegion
@ stub GdiDciBeginAccess
@ stub GdiDciCreateOffscreenSurface
@ stub GdiDciCreateOverlaySurface
@ stub GdiDciCreatePrimarySurface
@ stub GdiDciDestroySurface
@ stub GdiDciDrawSurface
@ stub GdiDciEndAccess
@ stub GdiDciEnumSurface
@ stub GdiDciInitialize
@ stub GdiDciSetClipList
@ stub GdiDciSetDestination
@ stub GdiDeleteLocalDC
@ stub GdiDeleteLocalObject
@ stub GdiDllInitialize
@ stdcall GdiFlush()
@ stdcall GdiGetBatchLimit()
@ stub GdiGetLocalBitmap
@ stub GdiGetLocalBrush
@ stub GdiGetLocalDC
@ stub GdiGetLocalFont
@ stdcall GdiGradientFill(long ptr long ptr long long) 
@ stub GdiIsMetaFileDC
@ stub GdiPlayDCScript
@ stub GdiPlayJournal
@ stub GdiPlayScript
@ stub GdiReleaseLocalDC
@ stub GdiSetAttrs
@ stdcall GdiSetBatchLimit(long)
@ stub GdiSetServerAttr
@ stdcall GdiTransparentBlt(long long long long long long long long long long long)
@ stub GdiWinWatchClose
@ stub GdiWinWatchDidStatusChange
@ stub GdiWinWatchGetClipList
@ stub GdiWinWatchOpen
@ stdcall GetArcDirection(long)
@ stdcall GetAspectRatioFilterEx(long ptr)
@ stdcall GetBitmapBits(long long ptr)
@ stdcall GetBitmapDimensionEx(long ptr)
@ stdcall GetBkColor(long)
@ stdcall GetBkMode(long)
@ stdcall GetBoundsRect(long ptr long)
@ stdcall GetBrushOrgEx(long ptr)
@ stdcall GetCharABCWidthsA(long long long ptr)
@ stdcall GetCharABCWidthsFloatA(long long long ptr)
@ stdcall GetCharABCWidthsFloatW(long long long ptr)
@ stdcall GetCharABCWidthsW(long long long ptr)
@ stdcall GetCharWidth32A(long long long long)
@ stdcall GetCharWidth32W(long long long long)
@ stdcall GetCharWidthA(long long long long) GetCharWidth32A
@ stdcall GetCharWidthFloatA(long long long ptr)
@ stdcall GetCharWidthFloatW(long long long ptr)
@ stdcall GetCharWidthW(long long long long) GetCharWidth32W
@ stub GetCharWidthWOW
@ stdcall GetCharacterPlacementA(long str long long ptr long)
@ stdcall GetCharacterPlacementW(long wstr long long ptr long)
@ stdcall GetClipBox(long ptr)
@ stdcall GetClipRgn(long long)
@ stdcall GetColorAdjustment(long ptr)
@ stdcall GetColorSpace(long)
@ stdcall GetCurrentObject(long long)
@ stdcall GetCurrentPositionEx(long ptr)
@ stdcall GetDCBrushColor(long)
@ stdcall GetDCOrgEx(long ptr)
@ stdcall GetDCPenColor(long)
@ stdcall GetDIBColorTable(long long long ptr)
@ stdcall GetDIBits(long long long long ptr ptr long)
@ stdcall GetDeviceCaps(long long)
@ stdcall GetDeviceGammaRamp(long ptr)
@ stub GetETM
@ stdcall GetEnhMetaFileA(str)
@ stdcall GetEnhMetaFileBits(long long ptr)
@ stdcall GetEnhMetaFileDescriptionA(long long ptr)
@ stdcall GetEnhMetaFileDescriptionW(long long ptr)
@ stdcall GetEnhMetaFileHeader(long long ptr)
@ stdcall GetEnhMetaFilePaletteEntries (long long ptr)
@ stdcall GetEnhMetaFileW(wstr)
@ stdcall GetFontData(long long long ptr long)
@ stdcall GetFontLanguageInfo(long)
@ stub GetFontResourceInfo
@ stub GetFontResourceInfoW
@ stdcall GetGlyphIndicesA(long ptr long ptr long)
@ stdcall GetGlyphIndicesW(long ptr long ptr long)
@ stub GetGlyphOutline
@ stdcall GetGlyphOutlineA(long long long ptr long ptr ptr)
@ stdcall GetGlyphOutlineW(long long long ptr long ptr ptr)
@ stub GetGlyphOutlineWow
@ stdcall GetGraphicsMode(long)
@ stdcall GetICMProfileA(long ptr ptr)
@ stub GetICMProfileW
@ stub GetKerningPairs
@ stdcall GetKerningPairsA(long long ptr)
@ stdcall GetKerningPairsW(long long ptr)
@ stdcall GetLayout(long)
@ stub GetLogColorSpaceA
@ stub GetLogColorSpaceW
@ stdcall GetMapMode(long)
@ stdcall GetMetaFileA(str)
@ stdcall GetMetaFileBitsEx(long long ptr)
@ stdcall GetMetaFileW(wstr)
@ stdcall GetMetaRgn(long long)
@ stdcall GetMiterLimit(long ptr)
@ stdcall GetNearestColor(long long)
@ stdcall GetNearestPaletteIndex(long long)
@ stdcall GetObjectA(long long ptr)
@ stdcall GetObjectType(long)
@ stdcall GetObjectW(long long ptr)
@ stdcall GetOutlineTextMetricsA(long long ptr)
@ stdcall GetOutlineTextMetricsW(long long ptr)
@ stdcall GetPaletteEntries(long long long ptr)
@ stdcall GetPath(long ptr ptr long)
@ stdcall GetPixel(long long long)
@ stdcall GetPixelFormat(long)
@ stdcall GetPolyFillMode(long)
@ stdcall GetROP2(long)
@ stdcall GetRandomRgn(long long long)
@ stdcall GetRasterizerCaps(ptr long)
@ stdcall GetRegionData(long long ptr)
@ stdcall GetRelAbs(long long)
@ stdcall GetRgnBox(long ptr)
@ stdcall GetStockObject(long)
@ stdcall GetStretchBltMode(long)
@ stdcall GetSystemPaletteEntries(long long long ptr)
@ stdcall GetSystemPaletteUse(long)
@ stdcall GetTextAlign(long)
@ stdcall GetTextCharacterExtra(long)
@ stdcall GetTextCharset(long)
@ stdcall GetTextCharsetInfo(long ptr long)
@ stdcall GetTextColor(long)
@ stdcall GetTextExtentExPointA(long str long long ptr ptr ptr)
@ stdcall GetTextExtentExPointW(long wstr long long ptr ptr ptr)
@ stdcall GetTextExtentPoint32A(long str long ptr)
@ stdcall GetTextExtentPoint32W(long wstr long ptr)
@ stdcall GetTextExtentPointA(long str long ptr)
@ stdcall GetTextExtentPointI(long ptr long ptr)
@ stdcall GetTextExtentPointW(long wstr long ptr)
@ stdcall GetTextFaceA(long long ptr)
@ stdcall GetTextFaceW(long long ptr)
@ stdcall GetTextMetricsA(long ptr)
@ stdcall GetTextMetricsW(long ptr)
@ stdcall GetTransform(long long ptr)
@ stdcall GetViewportExtEx(long ptr)
@ stdcall GetViewportOrgEx(long ptr)
@ stdcall GetWinMetaFileBits(long long ptr long long)
@ stdcall GetWindowExtEx(long ptr)
@ stdcall GetWindowOrgEx(long ptr)
@ stdcall GetWorldTransform(long ptr)
@ stdcall IntersectClipRect(long long long long long)
@ stdcall InvertRgn(long long)
@ stdcall LPtoDP(long ptr long)
@ stdcall LineDDA(long long long long ptr long)
@ stdcall LineTo(long long long)
@ stub LoadImageColorMatcherA
@ stub LoadImageColorMatcherW
@ stdcall MaskBlt(long long long long long long long long long long long long)
@ stdcall ModifyWorldTransform(long ptr long)
@ stdcall MoveToEx(long long long ptr)
@ stdcall OffsetClipRgn(long long long)
@ stdcall OffsetRgn(long long long)
@ stdcall OffsetViewportOrgEx(long long long ptr)
@ stdcall OffsetWindowOrgEx(long long long ptr)
@ stdcall PaintRgn(long long)
@ stdcall PatBlt(long long long long long long)
@ stdcall PathToRegion(long)
@ stdcall Pie(long long long long long long long long long)
@ stdcall PlayEnhMetaFile(long long ptr)
@ stdcall PlayEnhMetaFileRecord(long ptr ptr long)
@ stdcall PlayMetaFile(long long)
@ stdcall PlayMetaFileRecord(long ptr ptr long)
@ stdcall PlgBlt(long ptr long long long long long long long long)
@ stdcall PolyBezier(long ptr long)
@ stdcall PolyBezierTo(long ptr long)
@ stdcall PolyDraw(long ptr ptr long)
@ stdcall PolyPolygon(long ptr ptr long)
@ stdcall PolyPolyline(long ptr ptr long)
@ stdcall PolyTextOutA(long ptr long)
@ stdcall PolyTextOutW(long ptr long)
@ stdcall Polygon(long ptr long)
@ stdcall Polyline(long ptr long)
@ stdcall PolylineTo(long ptr long)
@ stdcall PtInRegion(long long long)
@ stdcall PtVisible(long long long)
@ stdcall RealizePalette(long)
@ stdcall RectInRegion(long ptr)
@ stdcall RectVisible(long ptr)
@ stdcall Rectangle(long long long long long)
@ stdcall RemoveFontResourceA(str)
@ stdcall RemoveFontResourceExA(str long ptr)
@ stdcall RemoveFontResourceExW(wstr long ptr)
@ stub RemoveFontResourceTracking
@ stdcall RemoveFontResourceW(wstr)
@ stdcall ResetDCA(long ptr)
@ stdcall ResetDCW(long ptr)
@ stdcall ResizePalette(long long)
@ stdcall RestoreDC(long long)
@ stdcall RoundRect(long long long long long long long)
@ stdcall SaveDC(long)
@ stdcall ScaleViewportExtEx(long long long long long ptr)
@ stdcall ScaleWindowExtEx(long long long long long ptr)
@ stub SelectBrushLocal
@ stdcall SelectClipPath(long long)
@ stdcall SelectClipRgn(long long)
@ stub SelectFontLocal
@ stdcall SelectObject(long long)
@ stdcall SelectPalette(long long long)
@ stdcall SetAbortProc(long ptr)
@ stdcall SetArcDirection(long long)
@ stdcall SetBitmapBits(long long ptr)
@ stdcall SetBitmapDimensionEx(long long long ptr)
@ stdcall SetBkColor(long long)
@ stdcall SetBkMode(long long)
@ stdcall SetBoundsRect(long ptr long)
@ stdcall SetBrushOrgEx(long long long ptr)
@ stdcall SetColorAdjustment(long ptr)
@ stdcall SetColorSpace(long long)
@ stdcall SetDCBrushColor(long long)
@ stdcall SetDCPenColor(long long)
@ stdcall SetDIBColorTable(long long long ptr)
@ stdcall SetDIBits(long long long long ptr ptr long)
@ stdcall SetDIBitsToDevice(long long long long long long long long long ptr ptr long)
@ stdcall SetDeviceGammaRamp(long ptr)
@ stdcall SetEnhMetaFileBits(long ptr)
@ stub SetFontEnumeration
@ stdcall SetGraphicsMode(long long)
@ stdcall SetICMMode(long long)
@ stdcall SetICMProfileA (long ptr)
@ stub SetICMProfileW
@ stdcall SetLayout(long long)
@ stub SetMagicColors
@ stdcall SetMapMode(long long)
@ stdcall SetMapperFlags(long long)
@ stdcall SetMetaFileBitsEx(long ptr)
@ stdcall SetMetaRgn(long)
@ stdcall SetMiterLimit(long long ptr)
@ stdcall SetObjectOwner(long long)
@ stdcall SetPaletteEntries(long long long ptr)
@ stdcall SetPixel(long long long long)
@ stdcall SetPixelFormat(long long ptr)
@ stdcall SetPixelV(long long long long)
@ stdcall SetPolyFillMode(long long)
@ stdcall SetROP2(long long)
@ stdcall SetRectRgn(long long long long long)
@ stdcall SetRelAbs(long long)
@ stdcall SetStretchBltMode(long long)
@ stdcall SetSystemPaletteUse(long long)
@ stdcall SetTextAlign(long long)
@ stdcall SetTextCharacterExtra(long long)
@ stdcall SetTextColor(long long)
@ stdcall SetTextJustification(long long long)
@ stdcall SetViewportExtEx(long long long ptr)
@ stdcall SetViewportOrgEx(long long long ptr)
@ stdcall SetVirtualResolution(long long long long long)
@ stdcall SetWinMetaFileBits(long ptr long ptr)
@ stdcall SetWindowExtEx(long long long ptr)
@ stdcall SetWindowOrgEx(long long long ptr)
@ stdcall SetWorldTransform(long ptr)
@ stdcall StartDocA(long ptr)
@ stdcall StartDocW(long ptr)
@ stdcall StartPage(long)
@ stdcall StretchBlt(long long long long long long long long long long long)
@ stdcall StretchDIBits(long long long long long long long long long ptr ptr long long)
@ stdcall StrokeAndFillPath(long)
@ stdcall StrokePath(long)
@ stdcall SwapBuffers(long)
@ stdcall TextOutA(long long long str long)
@ stdcall TextOutW(long long long wstr long)
@ stdcall TranslateCharsetInfo(ptr ptr long)
@ stub UnloadNetworkFonts
@ stdcall UnrealizeObject(long)
@ stdcall UpdateColors(long)
@ stub UpdateICMRegKey
@ stub UpdateICMRegKeyA
@ stub UpdateICMRegKeyW
@ stdcall WidenPath(long)
@ stub gdiPlaySpoolStream
@ extern pfnRealizePalette
@ extern pfnSelectPalette
@ stub pstackConnect

################################################################
# Wine extensions: Win16 functions that are needed by other dlls
#
@ stdcall CloseJob16(long)
@ stdcall CloseMetaFile16(long)
@ stdcall DeleteMetaFile16(long)
@ stdcall DrvGetPrinterData16(str str ptr ptr long ptr)
@ stdcall DrvSetPrinterData16(str str long ptr long)
@ stdcall ExcludeVisRect16(long long long long long)
@ stdcall GetDCState16(long)
@ stdcall IntersectVisRect16(long long long long long)
@ stdcall IsDCCurrentPalette16(long)
@ stdcall OpenJob16(str str long)
@ stdcall SelectVisRgn16(long long)
@ stdcall SetDCHook(long ptr long)
@ stdcall SetDCOrg16(long long long)
@ stdcall SetDCState16(long long)
@ stdcall SetHookFlags16(long long)
@ stdcall WriteSpool16(long ptr long)

################################################################
# Wine dll separation hacks, these will go away, don't use them
#
@ cdecl DIB_CreateDIBSection(long ptr long ptr long long long)
@ cdecl GDI_GetObjPtr(long long)
@ cdecl GDI_ReleaseObj(long)
