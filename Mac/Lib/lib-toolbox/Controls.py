# Generated from 'Macintosh HD:SWDev:Codewarrior 6 MPTP:Metrowerks CodeWarrior:MacOS Support:Universal:Interfaces:CIncludes:Controls.h'

def FOUR_CHAR_CODE(x): return x
from TextEdit import *
from QuickDraw import *

kControlDefProcType = FOUR_CHAR_CODE('CDEF')
kControlTemplateResourceType = FOUR_CHAR_CODE('CNTL')
kControlColorTableResourceType = FOUR_CHAR_CODE('cctb')
kControlDefProcResourceType = FOUR_CHAR_CODE('CDEF')
controlNotifyNothing = FOUR_CHAR_CODE('nada')
controlNotifyClick = FOUR_CHAR_CODE('clik')
controlNotifyFocus = FOUR_CHAR_CODE('focu')
controlNotifyKey = FOUR_CHAR_CODE('key ')        
kControlCanAutoInvalidate = 1L << 0                       
staticTextProc = 256
editTextProc = 272
iconProc = 288
userItemProc = 304
pictItemProc = 320                           
cFrameColor = 0
cBodyColor = 1
cTextColor = 2
cThumbColor = 3
kNumberCtlCTabEntries = 4
kControlNoVariant = 0
kControlUsesOwningWindowsFontVariant = 1 << 3               
kControlNoPart = 0
kControlIndicatorPart = 129
kControlDisabledPart = 254
kControlInactivePart = 255
kControlEntireControl = 0
kControlStructureMetaPart = -1
kControlContentMetaPart = -2
kControlFocusNoPart = 0
kControlFocusNextPart = -1
kControlFocusPrevPart = -2                            
kControlCollectionTagBounds = FOUR_CHAR_CODE('boun')
kControlCollectionTagValue = FOUR_CHAR_CODE('valu')
kControlCollectionTagMinimum = FOUR_CHAR_CODE('min ')
kControlCollectionTagMaximum = FOUR_CHAR_CODE('max ')
kControlCollectionTagViewSize = FOUR_CHAR_CODE('view')
kControlCollectionTagVisibility = FOUR_CHAR_CODE('visi')
kControlCollectionTagRefCon = FOUR_CHAR_CODE('refc')
kControlCollectionTagTitle = FOUR_CHAR_CODE('titl')
kControlCollectionTagIDSignature = FOUR_CHAR_CODE('idsi')
kControlCollectionTagIDID = FOUR_CHAR_CODE('idid')
kControlCollectionTagCommand = FOUR_CHAR_CODE('cmd ')       
kControlCollectionTagSubControls = FOUR_CHAR_CODE('subc')   
kControlContentTextOnly = 0
kControlNoContent = 0
kControlContentIconSuiteRes = 1
kControlContentCIconRes = 2
kControlContentPictRes = 3
kControlContentICONRes = 4
kControlContentIconSuiteHandle = 129
kControlContentCIconHandle = 130
kControlContentPictHandle = 131
kControlContentIconRef = 132
kControlContentICON = 133
kControlKeyScriptBehaviorAllowAnyScript = FOUR_CHAR_CODE('any ')
kControlKeyScriptBehaviorPrefersRoman = FOUR_CHAR_CODE('prmn')
kControlKeyScriptBehaviorRequiresRoman = FOUR_CHAR_CODE('rrmn') 
kControlFontBigSystemFont = -1
kControlFontSmallSystemFont = -2
kControlFontSmallBoldSystemFont = -3
kControlFontViewSystemFont = -4                            
kControlUseFontMask = 0x0001
kControlUseFaceMask = 0x0002
kControlUseSizeMask = 0x0004
kControlUseForeColorMask = 0x0008
kControlUseBackColorMask = 0x0010
kControlUseModeMask = 0x0020
kControlUseJustMask = 0x0040
kControlUseAllMask = 0x00FF
kControlAddFontSizeMask = 0x0100
kControlAddToMetaFontMask = 0x0200                        
kDoNotActivateAndIgnoreClick = 0
kDoNotActivateAndHandleClick = 1
kActivateAndIgnoreClick = 2
kActivateAndHandleClick = 3                             
kControlFontStyleTag = FOUR_CHAR_CODE('font')
kControlKeyFilterTag = FOUR_CHAR_CODE('fltr')
kControlSupportsGhosting = 1 << 0
kControlSupportsEmbedding = 1 << 1
kControlSupportsFocus = 1 << 2
kControlWantsIdle = 1 << 3
kControlWantsActivate = 1 << 4
kControlHandlesTracking = 1 << 5
kControlSupportsDataAccess = 1 << 6
kControlHasSpecialBackground = 1 << 7
kControlGetsFocusOnClick = 1 << 8
kControlSupportsCalcBestRect = 1 << 9
kControlSupportsLiveFeedback = 1 << 10
kControlHasRadioBehavior = 1 << 11
kControlSupportsDragAndDrop = 1 << 12
kControlAutoToggles = 1 << 14
kControlSupportsGetRegion = 1 << 17
kControlSupportsFlattening = 1 << 19
kControlSupportsSetCursor = 1 << 20
kControlSupportsContextualMenus = 1 << 21
kControlSupportsClickActivation = 1 << 22                   
drawCntl = 0
testCntl = 1
calcCRgns = 2
initCntl = 3
dispCntl = 4
posCntl = 5
thumbCntl = 6
dragCntl = 7
autoTrack = 8
calcCntlRgn = 10
calcThumbRgn = 11
drawThumbOutline = 12
kControlMsgDrawGhost = 13
kControlMsgCalcBestRect = 14
kControlMsgHandleTracking = 15
kControlMsgFocus = 16
kControlMsgKeyDown = 17
kControlMsgIdle = 18
kControlMsgGetFeatures = 19
kControlMsgSetData = 20
kControlMsgGetData = 21
kControlMsgActivate = 22
kControlMsgSetUpBackground = 23
kControlMsgCalcValueFromPos = 26
kControlMsgTestNewMsgSupport = 27
kControlMsgSubValueChanged = 25
kControlMsgSubControlAdded = 28
kControlMsgSubControlRemoved = 29
kControlMsgApplyTextColor = 30
kControlMsgGetRegion = 31
kControlMsgFlatten = 32
kControlMsgSetCursor = 33
kControlMsgDragEnter = 38
kControlMsgDragLeave = 39
kControlMsgDragWithin = 40
kControlMsgDragReceive = 41
kControlMsgDisplayDebugInfo = 46
kControlMsgContextualMenuClick = 47
kControlMsgGetClickActivation = 48                          
kDrawControlEntireControl = 0
kDrawControlIndicatorOnly = 129
kDragControlEntireControl = 0
kDragControlIndicator = 1
kControlSupportsNewMessages = FOUR_CHAR_CODE(' ok ')
kControlKeyFilterBlockKey = 0
kControlKeyFilterPassKey = 1
noConstraint = kNoConstraint
hAxisOnly = 1
vAxisOnly = 2
kControlDefProcPtr = 0                             
kControlPropertyPersistent = 0x00000001                    
kDragTrackingEnterControl = kDragTrackingEnterWindow
kDragTrackingInControl = kDragTrackingInWindow
kDragTrackingLeaveControl = kDragTrackingLeaveWindow
useWFont = kControlUsesOwningWindowsFontVariant
inThumb = kControlIndicatorPart
kNoHiliteControlPart = kControlNoPart
kInIndicatorControlPart = kControlIndicatorPart
kReservedControlPart = kControlDisabledPart
kControlInactiveControlPart = kControlInactivePart
