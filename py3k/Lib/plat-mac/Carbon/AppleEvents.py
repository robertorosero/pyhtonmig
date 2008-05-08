# Generated from 'AEDataModel.h'

def FOUR_CHAR_CODE(x): return x.encode("latin-1")
typeApplicationBundleID = FOUR_CHAR_CODE('bund')
typeBoolean = FOUR_CHAR_CODE('bool')
typeChar = FOUR_CHAR_CODE('TEXT')
typeSInt16 = FOUR_CHAR_CODE('shor')
typeSInt32 = FOUR_CHAR_CODE('long')
typeUInt32 = FOUR_CHAR_CODE('magn')
typeSInt64 = FOUR_CHAR_CODE('comp')
typeIEEE32BitFloatingPoint = FOUR_CHAR_CODE('sing')
typeIEEE64BitFloatingPoint = FOUR_CHAR_CODE('doub')
type128BitFloatingPoint = FOUR_CHAR_CODE('ldbl')
typeDecimalStruct = FOUR_CHAR_CODE('decm')
typeSMInt = typeSInt16
typeShortInteger = typeSInt16
typeInteger = typeSInt32
typeLongInteger = typeSInt32
typeMagnitude = typeUInt32
typeComp = typeSInt64
typeSMFloat = typeIEEE32BitFloatingPoint
typeShortFloat = typeIEEE32BitFloatingPoint
typeFloat = typeIEEE64BitFloatingPoint
typeLongFloat = typeIEEE64BitFloatingPoint
typeExtended = FOUR_CHAR_CODE('exte')
typeAEList = FOUR_CHAR_CODE('list')
typeAERecord = FOUR_CHAR_CODE('reco')
typeAppleEvent = FOUR_CHAR_CODE('aevt')
typeEventRecord = FOUR_CHAR_CODE('evrc')
typeTrue = FOUR_CHAR_CODE('true')
typeFalse = FOUR_CHAR_CODE('fals')
typeAlias = FOUR_CHAR_CODE('alis')
typeEnumerated = FOUR_CHAR_CODE('enum')
typeType = FOUR_CHAR_CODE('type')
typeAppParameters = FOUR_CHAR_CODE('appa')
typeProperty = FOUR_CHAR_CODE('prop')
typeFSS = FOUR_CHAR_CODE('fss ')
typeFSRef = FOUR_CHAR_CODE('fsrf')
typeFileURL = FOUR_CHAR_CODE('furl')
typeKeyword = FOUR_CHAR_CODE('keyw')
typeSectionH = FOUR_CHAR_CODE('sect')
typeWildCard = FOUR_CHAR_CODE('****')
typeApplSignature = FOUR_CHAR_CODE('sign')
typeQDRectangle = FOUR_CHAR_CODE('qdrt')
typeFixed = FOUR_CHAR_CODE('fixd')
typeProcessSerialNumber = FOUR_CHAR_CODE('psn ')
typeApplicationURL = FOUR_CHAR_CODE('aprl')
typeNull = FOUR_CHAR_CODE('null')
typeSessionID = FOUR_CHAR_CODE('ssid')
typeTargetID = FOUR_CHAR_CODE('targ')
typeDispatcherID = FOUR_CHAR_CODE('dspt')
keyTransactionIDAttr = FOUR_CHAR_CODE('tran')
keyReturnIDAttr = FOUR_CHAR_CODE('rtid')
keyEventClassAttr = FOUR_CHAR_CODE('evcl')
keyEventIDAttr = FOUR_CHAR_CODE('evid')
keyAddressAttr = FOUR_CHAR_CODE('addr')
keyOptionalKeywordAttr = FOUR_CHAR_CODE('optk')
keyTimeoutAttr = FOUR_CHAR_CODE('timo')
keyInteractLevelAttr = FOUR_CHAR_CODE('inte')
keyEventSourceAttr = FOUR_CHAR_CODE('esrc')
keyMissedKeywordAttr = FOUR_CHAR_CODE('miss')
keyOriginalAddressAttr = FOUR_CHAR_CODE('from')
keyAcceptTimeoutAttr = FOUR_CHAR_CODE('actm')
kAEDescListFactorNone = 0
kAEDescListFactorType = 4
kAEDescListFactorTypeAndSize = 8
kAutoGenerateReturnID = -1
kAnyTransactionID = 0
kAEDataArray = 0
kAEPackedArray = 1
kAEDescArray = 3
kAEKeyDescArray = 4
kAEHandleArray = 2
kAENormalPriority = 0x00000000
kAEHighPriority = 0x00000001
kAENoReply = 0x00000001
kAEQueueReply = 0x00000002
kAEWaitReply = 0x00000003
kAEDontReconnect = 0x00000080
kAEWantReceipt = 0x00000200
kAENeverInteract = 0x00000010
kAECanInteract = 0x00000020
kAEAlwaysInteract = 0x00000030
kAECanSwitchLayer = 0x00000040
kAEDontRecord = 0x00001000
kAEDontExecute = 0x00002000
kAEProcessNonReplyEvents = 0x00008000
kAEDefaultTimeout = -1
kNoTimeOut = -2
kAEInteractWithSelf = 0
kAEInteractWithLocal = 1
kAEInteractWithAll = 2
kAEDoNotIgnoreHandler = 0x00000000
kAEIgnoreAppPhacHandler = 0x00000001
kAEIgnoreAppEventHandler = 0x00000002
kAEIgnoreSysPhacHandler = 0x00000004
kAEIgnoreSysEventHandler = 0x00000008
kAEIngoreBuiltInEventHandler = 0x00000010
# kAEDontDisposeOnResume = (long)0x80000000
kAENoDispatch = 0
# kAEUseStandardDispatch = (long)0xFFFFFFFF
keyDirectObject = FOUR_CHAR_CODE('----')
keyErrorNumber = FOUR_CHAR_CODE('errn')
keyErrorString = FOUR_CHAR_CODE('errs')
keyProcessSerialNumber = FOUR_CHAR_CODE('psn ')
keyPreDispatch = FOUR_CHAR_CODE('phac')
keySelectProc = FOUR_CHAR_CODE('selh')
keyAERecorderCount = FOUR_CHAR_CODE('recr')
keyAEVersion = FOUR_CHAR_CODE('vers')
kCoreEventClass = FOUR_CHAR_CODE('aevt')
kAEOpenApplication = FOUR_CHAR_CODE('oapp')
kAEOpenDocuments = FOUR_CHAR_CODE('odoc')
kAEPrintDocuments = FOUR_CHAR_CODE('pdoc')
kAEQuitApplication = FOUR_CHAR_CODE('quit')
kAEAnswer = FOUR_CHAR_CODE('ansr')
kAEApplicationDied = FOUR_CHAR_CODE('obit')
kAEShowPreferences = FOUR_CHAR_CODE('pref')
kAEStartRecording = FOUR_CHAR_CODE('reca')
kAEStopRecording = FOUR_CHAR_CODE('recc')
kAENotifyStartRecording = FOUR_CHAR_CODE('rec1')
kAENotifyStopRecording = FOUR_CHAR_CODE('rec0')
kAENotifyRecording = FOUR_CHAR_CODE('recr')
kAEUnknownSource = 0
kAEDirectCall = 1
kAESameProcess = 2
kAELocalProcess = 3
kAERemoteProcess = 4
cAEList = FOUR_CHAR_CODE('list')
cApplication = FOUR_CHAR_CODE('capp')
cArc = FOUR_CHAR_CODE('carc')
cBoolean = FOUR_CHAR_CODE('bool')
cCell = FOUR_CHAR_CODE('ccel')
cChar = FOUR_CHAR_CODE('cha ')
cColorTable = FOUR_CHAR_CODE('clrt')
cColumn = FOUR_CHAR_CODE('ccol')
cDocument = FOUR_CHAR_CODE('docu')
cDrawingArea = FOUR_CHAR_CODE('cdrw')
cEnumeration = FOUR_CHAR_CODE('enum')
cFile = FOUR_CHAR_CODE('file')
cFixed = FOUR_CHAR_CODE('fixd')
cFixedPoint = FOUR_CHAR_CODE('fpnt')
cFixedRectangle = FOUR_CHAR_CODE('frct')
cGraphicLine = FOUR_CHAR_CODE('glin')
cGraphicObject = FOUR_CHAR_CODE('cgob')
cGraphicShape = FOUR_CHAR_CODE('cgsh')
cGraphicText = FOUR_CHAR_CODE('cgtx')
cGroupedGraphic = FOUR_CHAR_CODE('cpic')
cInsertionLoc = FOUR_CHAR_CODE('insl')
cInsertionPoint = FOUR_CHAR_CODE('cins')
cIntlText = FOUR_CHAR_CODE('itxt')
cIntlWritingCode = FOUR_CHAR_CODE('intl')
cItem = FOUR_CHAR_CODE('citm')
cLine = FOUR_CHAR_CODE('clin')
cLongDateTime = FOUR_CHAR_CODE('ldt ')
cLongFixed = FOUR_CHAR_CODE('lfxd')
cLongFixedPoint = FOUR_CHAR_CODE('lfpt')
cLongFixedRectangle = FOUR_CHAR_CODE('lfrc')
cLongInteger = FOUR_CHAR_CODE('long')
cLongPoint = FOUR_CHAR_CODE('lpnt')
cLongRectangle = FOUR_CHAR_CODE('lrct')
cMachineLoc = FOUR_CHAR_CODE('mLoc')
cMenu = FOUR_CHAR_CODE('cmnu')
cMenuItem = FOUR_CHAR_CODE('cmen')
cObject = FOUR_CHAR_CODE('cobj')
cObjectSpecifier = FOUR_CHAR_CODE('obj ')
cOpenableObject = FOUR_CHAR_CODE('coob')
cOval = FOUR_CHAR_CODE('covl')
cParagraph = FOUR_CHAR_CODE('cpar')
cPICT = FOUR_CHAR_CODE('PICT')
cPixel = FOUR_CHAR_CODE('cpxl')
cPixelMap = FOUR_CHAR_CODE('cpix')
cPolygon = FOUR_CHAR_CODE('cpgn')
cProperty = FOUR_CHAR_CODE('prop')
cQDPoint = FOUR_CHAR_CODE('QDpt')
cQDRectangle = FOUR_CHAR_CODE('qdrt')
cRectangle = FOUR_CHAR_CODE('crec')
cRGBColor = FOUR_CHAR_CODE('cRGB')
cRotation = FOUR_CHAR_CODE('trot')
cRoundedRectangle = FOUR_CHAR_CODE('crrc')
cRow = FOUR_CHAR_CODE('crow')
cSelection = FOUR_CHAR_CODE('csel')
cShortInteger = FOUR_CHAR_CODE('shor')
cTable = FOUR_CHAR_CODE('ctbl')
cText = FOUR_CHAR_CODE('ctxt')
cTextFlow = FOUR_CHAR_CODE('cflo')
cTextStyles = FOUR_CHAR_CODE('tsty')
cType = FOUR_CHAR_CODE('type')
cVersion = FOUR_CHAR_CODE('vers')
cWindow = FOUR_CHAR_CODE('cwin')
cWord = FOUR_CHAR_CODE('cwor')
enumArrows = FOUR_CHAR_CODE('arro')
enumJustification = FOUR_CHAR_CODE('just')
enumKeyForm = FOUR_CHAR_CODE('kfrm')
enumPosition = FOUR_CHAR_CODE('posi')
enumProtection = FOUR_CHAR_CODE('prtn')
enumQuality = FOUR_CHAR_CODE('qual')
enumSaveOptions = FOUR_CHAR_CODE('savo')
enumStyle = FOUR_CHAR_CODE('styl')
enumTransferMode = FOUR_CHAR_CODE('tran')
formUniqueID = FOUR_CHAR_CODE('ID  ')
kAEAbout = FOUR_CHAR_CODE('abou')
kAEAfter = FOUR_CHAR_CODE('afte')
kAEAliasSelection = FOUR_CHAR_CODE('sali')
kAEAllCaps = FOUR_CHAR_CODE('alcp')
kAEArrowAtEnd = FOUR_CHAR_CODE('aren')
kAEArrowAtStart = FOUR_CHAR_CODE('arst')
kAEArrowBothEnds = FOUR_CHAR_CODE('arbo')
kAEAsk = FOUR_CHAR_CODE('ask ')
kAEBefore = FOUR_CHAR_CODE('befo')
kAEBeginning = FOUR_CHAR_CODE('bgng')
kAEBeginsWith = FOUR_CHAR_CODE('bgwt')
kAEBeginTransaction = FOUR_CHAR_CODE('begi')
kAEBold = FOUR_CHAR_CODE('bold')
kAECaseSensEquals = FOUR_CHAR_CODE('cseq')
kAECentered = FOUR_CHAR_CODE('cent')
kAEChangeView = FOUR_CHAR_CODE('view')
kAEClone = FOUR_CHAR_CODE('clon')
kAEClose = FOUR_CHAR_CODE('clos')
kAECondensed = FOUR_CHAR_CODE('cond')
kAEContains = FOUR_CHAR_CODE('cont')
kAECopy = FOUR_CHAR_CODE('copy')
kAECoreSuite = FOUR_CHAR_CODE('core')
kAECountElements = FOUR_CHAR_CODE('cnte')
kAECreateElement = FOUR_CHAR_CODE('crel')
kAECreatePublisher = FOUR_CHAR_CODE('cpub')
kAECut = FOUR_CHAR_CODE('cut ')
kAEDelete = FOUR_CHAR_CODE('delo')
kAEDoObjectsExist = FOUR_CHAR_CODE('doex')
kAEDoScript = FOUR_CHAR_CODE('dosc')
kAEDrag = FOUR_CHAR_CODE('drag')
kAEDuplicateSelection = FOUR_CHAR_CODE('sdup')
kAEEditGraphic = FOUR_CHAR_CODE('edit')
kAEEmptyTrash = FOUR_CHAR_CODE('empt')
kAEEnd = FOUR_CHAR_CODE('end ')
kAEEndsWith = FOUR_CHAR_CODE('ends')
kAEEndTransaction = FOUR_CHAR_CODE('endt')
kAEEquals = FOUR_CHAR_CODE('=   ')
kAEExpanded = FOUR_CHAR_CODE('pexp')
kAEFast = FOUR_CHAR_CODE('fast')
kAEFinderEvents = FOUR_CHAR_CODE('FNDR')
kAEFormulaProtect = FOUR_CHAR_CODE('fpro')
kAEFullyJustified = FOUR_CHAR_CODE('full')
kAEGetClassInfo = FOUR_CHAR_CODE('qobj')
kAEGetData = FOUR_CHAR_CODE('getd')
kAEGetDataSize = FOUR_CHAR_CODE('dsiz')
kAEGetEventInfo = FOUR_CHAR_CODE('gtei')
kAEGetInfoSelection = FOUR_CHAR_CODE('sinf')
kAEGetPrivilegeSelection = FOUR_CHAR_CODE('sprv')
kAEGetSuiteInfo = FOUR_CHAR_CODE('gtsi')
kAEGreaterThan = FOUR_CHAR_CODE('>   ')
kAEGreaterThanEquals = FOUR_CHAR_CODE('>=  ')
kAEGrow = FOUR_CHAR_CODE('grow')
kAEHidden = FOUR_CHAR_CODE('hidn')
kAEHiQuality = FOUR_CHAR_CODE('hiqu')
kAEImageGraphic = FOUR_CHAR_CODE('imgr')
kAEIsUniform = FOUR_CHAR_CODE('isun')
kAEItalic = FOUR_CHAR_CODE('ital')
kAELeftJustified = FOUR_CHAR_CODE('left')
kAELessThan = FOUR_CHAR_CODE('<   ')
kAELessThanEquals = FOUR_CHAR_CODE('<=  ')
kAELowercase = FOUR_CHAR_CODE('lowc')
kAEMakeObjectsVisible = FOUR_CHAR_CODE('mvis')
kAEMiscStandards = FOUR_CHAR_CODE('misc')
kAEModifiable = FOUR_CHAR_CODE('modf')
kAEMove = FOUR_CHAR_CODE('move')
kAENo = FOUR_CHAR_CODE('no  ')
kAENoArrow = FOUR_CHAR_CODE('arno')
kAENonmodifiable = FOUR_CHAR_CODE('nmod')
kAEOpen = FOUR_CHAR_CODE('odoc')
kAEOpenSelection = FOUR_CHAR_CODE('sope')
kAEOutline = FOUR_CHAR_CODE('outl')
kAEPageSetup = FOUR_CHAR_CODE('pgsu')
kAEPaste = FOUR_CHAR_CODE('past')
kAEPlain = FOUR_CHAR_CODE('plan')
kAEPrint = FOUR_CHAR_CODE('pdoc')
kAEPrintSelection = FOUR_CHAR_CODE('spri')
kAEPrintWindow = FOUR_CHAR_CODE('pwin')
kAEPutAwaySelection = FOUR_CHAR_CODE('sput')
kAEQDAddOver = FOUR_CHAR_CODE('addo')
kAEQDAddPin = FOUR_CHAR_CODE('addp')
kAEQDAdMax = FOUR_CHAR_CODE('admx')
kAEQDAdMin = FOUR_CHAR_CODE('admn')
kAEQDBic = FOUR_CHAR_CODE('bic ')
kAEQDBlend = FOUR_CHAR_CODE('blnd')
kAEQDCopy = FOUR_CHAR_CODE('cpy ')
kAEQDNotBic = FOUR_CHAR_CODE('nbic')
kAEQDNotCopy = FOUR_CHAR_CODE('ncpy')
kAEQDNotOr = FOUR_CHAR_CODE('ntor')
kAEQDNotXor = FOUR_CHAR_CODE('nxor')
kAEQDOr = FOUR_CHAR_CODE('or  ')
kAEQDSubOver = FOUR_CHAR_CODE('subo')
kAEQDSubPin = FOUR_CHAR_CODE('subp')
kAEQDSupplementalSuite = FOUR_CHAR_CODE('qdsp')
kAEQDXor = FOUR_CHAR_CODE('xor ')
kAEQuickdrawSuite = FOUR_CHAR_CODE('qdrw')
kAEQuitAll = FOUR_CHAR_CODE('quia')
kAERedo = FOUR_CHAR_CODE('redo')
kAERegular = FOUR_CHAR_CODE('regl')
kAEReopenApplication = FOUR_CHAR_CODE('rapp')
kAEReplace = FOUR_CHAR_CODE('rplc')
kAERequiredSuite = FOUR_CHAR_CODE('reqd')
kAERestart = FOUR_CHAR_CODE('rest')
kAERevealSelection = FOUR_CHAR_CODE('srev')
kAERevert = FOUR_CHAR_CODE('rvrt')
kAERightJustified = FOUR_CHAR_CODE('rght')
kAESave = FOUR_CHAR_CODE('save')
kAESelect = FOUR_CHAR_CODE('slct')
kAESetData = FOUR_CHAR_CODE('setd')
kAESetPosition = FOUR_CHAR_CODE('posn')
kAEShadow = FOUR_CHAR_CODE('shad')
kAEShowClipboard = FOUR_CHAR_CODE('shcl')
kAEShutDown = FOUR_CHAR_CODE('shut')
kAESleep = FOUR_CHAR_CODE('slep')
kAESmallCaps = FOUR_CHAR_CODE('smcp')
kAESpecialClassProperties = FOUR_CHAR_CODE('c@#!')
kAEStrikethrough = FOUR_CHAR_CODE('strk')
kAESubscript = FOUR_CHAR_CODE('sbsc')
kAESuperscript = FOUR_CHAR_CODE('spsc')
kAETableSuite = FOUR_CHAR_CODE('tbls')
kAETextSuite = FOUR_CHAR_CODE('TEXT')
kAETransactionTerminated = FOUR_CHAR_CODE('ttrm')
kAEUnderline = FOUR_CHAR_CODE('undl')
kAEUndo = FOUR_CHAR_CODE('undo')
kAEWholeWordEquals = FOUR_CHAR_CODE('wweq')
kAEYes = FOUR_CHAR_CODE('yes ')
kAEZoom = FOUR_CHAR_CODE('zoom')
kAEMouseClass = FOUR_CHAR_CODE('mous')
kAEDown = FOUR_CHAR_CODE('down')
kAEUp = FOUR_CHAR_CODE('up  ')
kAEMoved = FOUR_CHAR_CODE('move')
kAEStoppedMoving = FOUR_CHAR_CODE('stop')
kAEWindowClass = FOUR_CHAR_CODE('wind')
kAEUpdate = FOUR_CHAR_CODE('updt')
kAEActivate = FOUR_CHAR_CODE('actv')
kAEDeactivate = FOUR_CHAR_CODE('dact')
kAECommandClass = FOUR_CHAR_CODE('cmnd')
kAEKeyClass = FOUR_CHAR_CODE('keyc')
kAERawKey = FOUR_CHAR_CODE('rkey')
kAEVirtualKey = FOUR_CHAR_CODE('keyc')
kAENavigationKey = FOUR_CHAR_CODE('nave')
kAEAutoDown = FOUR_CHAR_CODE('auto')
kAEApplicationClass = FOUR_CHAR_CODE('appl')
kAESuspend = FOUR_CHAR_CODE('susp')
kAEResume = FOUR_CHAR_CODE('rsme')
kAEDiskEvent = FOUR_CHAR_CODE('disk')
kAENullEvent = FOUR_CHAR_CODE('null')
kAEWakeUpEvent = FOUR_CHAR_CODE('wake')
kAEScrapEvent = FOUR_CHAR_CODE('scrp')
kAEHighLevel = FOUR_CHAR_CODE('high')
keyAEAngle = FOUR_CHAR_CODE('kang')
keyAEArcAngle = FOUR_CHAR_CODE('parc')
keyAEBaseAddr = FOUR_CHAR_CODE('badd')
keyAEBestType = FOUR_CHAR_CODE('pbst')
keyAEBgndColor = FOUR_CHAR_CODE('kbcl')
keyAEBgndPattern = FOUR_CHAR_CODE('kbpt')
keyAEBounds = FOUR_CHAR_CODE('pbnd')
keyAECellList = FOUR_CHAR_CODE('kclt')
keyAEClassID = FOUR_CHAR_CODE('clID')
keyAEColor = FOUR_CHAR_CODE('colr')
keyAEColorTable = FOUR_CHAR_CODE('cltb')
keyAECurveHeight = FOUR_CHAR_CODE('kchd')
keyAECurveWidth = FOUR_CHAR_CODE('kcwd')
keyAEDashStyle = FOUR_CHAR_CODE('pdst')
keyAEData = FOUR_CHAR_CODE('data')
keyAEDefaultType = FOUR_CHAR_CODE('deft')
keyAEDefinitionRect = FOUR_CHAR_CODE('pdrt')
keyAEDescType = FOUR_CHAR_CODE('dstp')
keyAEDestination = FOUR_CHAR_CODE('dest')
keyAEDoAntiAlias = FOUR_CHAR_CODE('anta')
keyAEDoDithered = FOUR_CHAR_CODE('gdit')
keyAEDoRotate = FOUR_CHAR_CODE('kdrt')
keyAEDoScale = FOUR_CHAR_CODE('ksca')
keyAEDoTranslate = FOUR_CHAR_CODE('ktra')
keyAEEditionFileLoc = FOUR_CHAR_CODE('eloc')
keyAEElements = FOUR_CHAR_CODE('elms')
keyAEEndPoint = FOUR_CHAR_CODE('pend')
keyAEEventClass = FOUR_CHAR_CODE('evcl')
keyAEEventID = FOUR_CHAR_CODE('evti')
keyAEFile = FOUR_CHAR_CODE('kfil')
keyAEFileType = FOUR_CHAR_CODE('fltp')
keyAEFillColor = FOUR_CHAR_CODE('flcl')
keyAEFillPattern = FOUR_CHAR_CODE('flpt')
keyAEFlipHorizontal = FOUR_CHAR_CODE('kfho')
keyAEFlipVertical = FOUR_CHAR_CODE('kfvt')
keyAEFont = FOUR_CHAR_CODE('font')
keyAEFormula = FOUR_CHAR_CODE('pfor')
keyAEGraphicObjects = FOUR_CHAR_CODE('gobs')
keyAEID = FOUR_CHAR_CODE('ID  ')
keyAEImageQuality = FOUR_CHAR_CODE('gqua')
keyAEInsertHere = FOUR_CHAR_CODE('insh')
keyAEKeyForms = FOUR_CHAR_CODE('keyf')
keyAEKeyword = FOUR_CHAR_CODE('kywd')
keyAELevel = FOUR_CHAR_CODE('levl')
keyAELineArrow = FOUR_CHAR_CODE('arro')
keyAEName = FOUR_CHAR_CODE('pnam')
keyAENewElementLoc = FOUR_CHAR_CODE('pnel')
keyAEObject = FOUR_CHAR_CODE('kobj')
keyAEObjectClass = FOUR_CHAR_CODE('kocl')
keyAEOffStyles = FOUR_CHAR_CODE('ofst')
keyAEOnStyles = FOUR_CHAR_CODE('onst')
keyAEParameters = FOUR_CHAR_CODE('prms')
keyAEParamFlags = FOUR_CHAR_CODE('pmfg')
keyAEPenColor = FOUR_CHAR_CODE('ppcl')
keyAEPenPattern = FOUR_CHAR_CODE('pppa')
keyAEPenWidth = FOUR_CHAR_CODE('ppwd')
keyAEPixelDepth = FOUR_CHAR_CODE('pdpt')
keyAEPixMapMinus = FOUR_CHAR_CODE('kpmm')
keyAEPMTable = FOUR_CHAR_CODE('kpmt')
keyAEPointList = FOUR_CHAR_CODE('ptlt')
keyAEPointSize = FOUR_CHAR_CODE('ptsz')
keyAEPosition = FOUR_CHAR_CODE('kpos')
keyAEPropData = FOUR_CHAR_CODE('prdt')
keyAEProperties = FOUR_CHAR_CODE('qpro')
keyAEProperty = FOUR_CHAR_CODE('kprp')
keyAEPropFlags = FOUR_CHAR_CODE('prfg')
keyAEPropID = FOUR_CHAR_CODE('prop')
keyAEProtection = FOUR_CHAR_CODE('ppro')
keyAERenderAs = FOUR_CHAR_CODE('kren')
keyAERequestedType = FOUR_CHAR_CODE('rtyp')
keyAEResult = FOUR_CHAR_CODE('----')
keyAEResultInfo = FOUR_CHAR_CODE('rsin')
keyAERotation = FOUR_CHAR_CODE('prot')
keyAERotPoint = FOUR_CHAR_CODE('krtp')
keyAERowList = FOUR_CHAR_CODE('krls')
keyAESaveOptions = FOUR_CHAR_CODE('savo')
keyAEScale = FOUR_CHAR_CODE('pscl')
keyAEScriptTag = FOUR_CHAR_CODE('psct')
keyAEShowWhere = FOUR_CHAR_CODE('show')
keyAEStartAngle = FOUR_CHAR_CODE('pang')
keyAEStartPoint = FOUR_CHAR_CODE('pstp')
keyAEStyles = FOUR_CHAR_CODE('ksty')
keyAESuiteID = FOUR_CHAR_CODE('suit')
keyAEText = FOUR_CHAR_CODE('ktxt')
keyAETextColor = FOUR_CHAR_CODE('ptxc')
keyAETextFont = FOUR_CHAR_CODE('ptxf')
keyAETextPointSize = FOUR_CHAR_CODE('ptps')
keyAETextStyles = FOUR_CHAR_CODE('txst')
keyAETextLineHeight = FOUR_CHAR_CODE('ktlh')
keyAETextLineAscent = FOUR_CHAR_CODE('ktas')
keyAETheText = FOUR_CHAR_CODE('thtx')
keyAETransferMode = FOUR_CHAR_CODE('pptm')
keyAETranslation = FOUR_CHAR_CODE('ptrs')
keyAETryAsStructGraf = FOUR_CHAR_CODE('toog')
keyAEUniformStyles = FOUR_CHAR_CODE('ustl')
keyAEUpdateOn = FOUR_CHAR_CODE('pupd')
keyAEUserTerm = FOUR_CHAR_CODE('utrm')
keyAEWindow = FOUR_CHAR_CODE('wndw')
keyAEWritingCode = FOUR_CHAR_CODE('wrcd')
keyMiscellaneous = FOUR_CHAR_CODE('fmsc')
keySelection = FOUR_CHAR_CODE('fsel')
keyWindow = FOUR_CHAR_CODE('kwnd')
keyWhen = FOUR_CHAR_CODE('when')
keyWhere = FOUR_CHAR_CODE('wher')
keyModifiers = FOUR_CHAR_CODE('mods')
keyKey = FOUR_CHAR_CODE('key ')
keyKeyCode = FOUR_CHAR_CODE('code')
keyKeyboard = FOUR_CHAR_CODE('keyb')
keyDriveNumber = FOUR_CHAR_CODE('drv#')
keyErrorCode = FOUR_CHAR_CODE('err#')
keyHighLevelClass = FOUR_CHAR_CODE('hcls')
keyHighLevelID = FOUR_CHAR_CODE('hid ')
pArcAngle = FOUR_CHAR_CODE('parc')
pBackgroundColor = FOUR_CHAR_CODE('pbcl')
pBackgroundPattern = FOUR_CHAR_CODE('pbpt')
pBestType = FOUR_CHAR_CODE('pbst')
pBounds = FOUR_CHAR_CODE('pbnd')
pClass = FOUR_CHAR_CODE('pcls')
pClipboard = FOUR_CHAR_CODE('pcli')
pColor = FOUR_CHAR_CODE('colr')
pColorTable = FOUR_CHAR_CODE('cltb')
pContents = FOUR_CHAR_CODE('pcnt')
pCornerCurveHeight = FOUR_CHAR_CODE('pchd')
pCornerCurveWidth = FOUR_CHAR_CODE('pcwd')
pDashStyle = FOUR_CHAR_CODE('pdst')
pDefaultType = FOUR_CHAR_CODE('deft')
pDefinitionRect = FOUR_CHAR_CODE('pdrt')
pEnabled = FOUR_CHAR_CODE('enbl')
pEndPoint = FOUR_CHAR_CODE('pend')
pFillColor = FOUR_CHAR_CODE('flcl')
pFillPattern = FOUR_CHAR_CODE('flpt')
pFont = FOUR_CHAR_CODE('font')
pFormula = FOUR_CHAR_CODE('pfor')
pGraphicObjects = FOUR_CHAR_CODE('gobs')
pHasCloseBox = FOUR_CHAR_CODE('hclb')
pHasTitleBar = FOUR_CHAR_CODE('ptit')
pID = FOUR_CHAR_CODE('ID  ')
pIndex = FOUR_CHAR_CODE('pidx')
pInsertionLoc = FOUR_CHAR_CODE('pins')
pIsFloating = FOUR_CHAR_CODE('isfl')
pIsFrontProcess = FOUR_CHAR_CODE('pisf')
pIsModal = FOUR_CHAR_CODE('pmod')
pIsModified = FOUR_CHAR_CODE('imod')
pIsResizable = FOUR_CHAR_CODE('prsz')
pIsStationeryPad = FOUR_CHAR_CODE('pspd')
pIsZoomable = FOUR_CHAR_CODE('iszm')
pIsZoomed = FOUR_CHAR_CODE('pzum')
pItemNumber = FOUR_CHAR_CODE('itmn')
pJustification = FOUR_CHAR_CODE('pjst')
pLineArrow = FOUR_CHAR_CODE('arro')
pMenuID = FOUR_CHAR_CODE('mnid')
pName = FOUR_CHAR_CODE('pnam')
pNewElementLoc = FOUR_CHAR_CODE('pnel')
pPenColor = FOUR_CHAR_CODE('ppcl')
pPenPattern = FOUR_CHAR_CODE('pppa')
pPenWidth = FOUR_CHAR_CODE('ppwd')
pPixelDepth = FOUR_CHAR_CODE('pdpt')
pPointList = FOUR_CHAR_CODE('ptlt')
pPointSize = FOUR_CHAR_CODE('ptsz')
pProtection = FOUR_CHAR_CODE('ppro')
pRotation = FOUR_CHAR_CODE('prot')
pScale = FOUR_CHAR_CODE('pscl')
pScript = FOUR_CHAR_CODE('scpt')
pScriptTag = FOUR_CHAR_CODE('psct')
pSelected = FOUR_CHAR_CODE('selc')
pSelection = FOUR_CHAR_CODE('sele')
pStartAngle = FOUR_CHAR_CODE('pang')
pStartPoint = FOUR_CHAR_CODE('pstp')
pTextColor = FOUR_CHAR_CODE('ptxc')
pTextFont = FOUR_CHAR_CODE('ptxf')
pTextItemDelimiters = FOUR_CHAR_CODE('txdl')
pTextPointSize = FOUR_CHAR_CODE('ptps')
pTextStyles = FOUR_CHAR_CODE('txst')
pTransferMode = FOUR_CHAR_CODE('pptm')
pTranslation = FOUR_CHAR_CODE('ptrs')
pUniformStyles = FOUR_CHAR_CODE('ustl')
pUpdateOn = FOUR_CHAR_CODE('pupd')
pUserSelection = FOUR_CHAR_CODE('pusl')
pVersion = FOUR_CHAR_CODE('vers')
pVisible = FOUR_CHAR_CODE('pvis')
typeAEText = FOUR_CHAR_CODE('tTXT')
typeArc = FOUR_CHAR_CODE('carc')
typeBest = FOUR_CHAR_CODE('best')
typeCell = FOUR_CHAR_CODE('ccel')
typeClassInfo = FOUR_CHAR_CODE('gcli')
typeColorTable = FOUR_CHAR_CODE('clrt')
typeColumn = FOUR_CHAR_CODE('ccol')
typeDashStyle = FOUR_CHAR_CODE('tdas')
typeData = FOUR_CHAR_CODE('tdta')
typeDrawingArea = FOUR_CHAR_CODE('cdrw')
typeElemInfo = FOUR_CHAR_CODE('elin')
typeEnumeration = FOUR_CHAR_CODE('enum')
typeEPS = FOUR_CHAR_CODE('EPS ')
typeEventInfo = FOUR_CHAR_CODE('evin')
typeFinderWindow = FOUR_CHAR_CODE('fwin')
typeFixedPoint = FOUR_CHAR_CODE('fpnt')
typeFixedRectangle = FOUR_CHAR_CODE('frct')
typeGraphicLine = FOUR_CHAR_CODE('glin')
typeGraphicText = FOUR_CHAR_CODE('cgtx')
typeGroupedGraphic = FOUR_CHAR_CODE('cpic')
typeInsertionLoc = FOUR_CHAR_CODE('insl')
typeIntlText = FOUR_CHAR_CODE('itxt')
typeIntlWritingCode = FOUR_CHAR_CODE('intl')
typeLongDateTime = FOUR_CHAR_CODE('ldt ')
typeLongFixed = FOUR_CHAR_CODE('lfxd')
typeLongFixedPoint = FOUR_CHAR_CODE('lfpt')
typeLongFixedRectangle = FOUR_CHAR_CODE('lfrc')
typeLongPoint = FOUR_CHAR_CODE('lpnt')
typeLongRectangle = FOUR_CHAR_CODE('lrct')
typeMachineLoc = FOUR_CHAR_CODE('mLoc')
typeOval = FOUR_CHAR_CODE('covl')
typeParamInfo = FOUR_CHAR_CODE('pmin')
typePict = FOUR_CHAR_CODE('PICT')
typePixelMap = FOUR_CHAR_CODE('cpix')
typePixMapMinus = FOUR_CHAR_CODE('tpmm')
typePolygon = FOUR_CHAR_CODE('cpgn')
typePropInfo = FOUR_CHAR_CODE('pinf')
typePtr = FOUR_CHAR_CODE('ptr ')
typeQDPoint = FOUR_CHAR_CODE('QDpt')
typeQDRegion = FOUR_CHAR_CODE('Qrgn')
typeRectangle = FOUR_CHAR_CODE('crec')
typeRGB16 = FOUR_CHAR_CODE('tr16')
typeRGB96 = FOUR_CHAR_CODE('tr96')
typeRGBColor = FOUR_CHAR_CODE('cRGB')
typeRotation = FOUR_CHAR_CODE('trot')
typeRoundedRectangle = FOUR_CHAR_CODE('crrc')
typeRow = FOUR_CHAR_CODE('crow')
typeScrapStyles = FOUR_CHAR_CODE('styl')
typeScript = FOUR_CHAR_CODE('scpt')
typeStyledText = FOUR_CHAR_CODE('STXT')
typeSuiteInfo = FOUR_CHAR_CODE('suin')
typeTable = FOUR_CHAR_CODE('ctbl')
typeTextStyles = FOUR_CHAR_CODE('tsty')
typeTIFF = FOUR_CHAR_CODE('TIFF')
typeVersion = FOUR_CHAR_CODE('vers')
kAEMenuClass = FOUR_CHAR_CODE('menu')
kAEMenuSelect = FOUR_CHAR_CODE('mhit')
kAEMouseDown = FOUR_CHAR_CODE('mdwn')
kAEMouseDownInBack = FOUR_CHAR_CODE('mdbk')
kAEKeyDown = FOUR_CHAR_CODE('kdwn')
kAEResized = FOUR_CHAR_CODE('rsiz')
kAEPromise = FOUR_CHAR_CODE('prom')
keyMenuID = FOUR_CHAR_CODE('mid ')
keyMenuItem = FOUR_CHAR_CODE('mitm')
keyCloseAllWindows = FOUR_CHAR_CODE('caw ')
keyOriginalBounds = FOUR_CHAR_CODE('obnd')
keyNewBounds = FOUR_CHAR_CODE('nbnd')
keyLocalWhere = FOUR_CHAR_CODE('lwhr')
typeHIMenu = FOUR_CHAR_CODE('mobj')
typeHIWindow = FOUR_CHAR_CODE('wobj')
kBySmallIcon = 0
kByIconView = 1
kByNameView = 2
kByDateView = 3
kBySizeView = 4
kByKindView = 5
kByCommentView = 6
kByLabelView = 7
kByVersionView = 8
kAEInfo = 11
kAEMain = 0
kAESharing = 13
kAEZoomIn = 7
kAEZoomOut = 8
kTextServiceClass = FOUR_CHAR_CODE('tsvc')
kUpdateActiveInputArea = FOUR_CHAR_CODE('updt')
kShowHideInputWindow = FOUR_CHAR_CODE('shiw')
kPos2Offset = FOUR_CHAR_CODE('p2st')
kOffset2Pos = FOUR_CHAR_CODE('st2p')
kUnicodeNotFromInputMethod = FOUR_CHAR_CODE('unim')
kGetSelectedText = FOUR_CHAR_CODE('gtxt')
keyAETSMDocumentRefcon = FOUR_CHAR_CODE('refc')
keyAEServerInstance = FOUR_CHAR_CODE('srvi')
keyAETheData = FOUR_CHAR_CODE('kdat')
keyAEFixLength = FOUR_CHAR_CODE('fixl')
keyAEUpdateRange = FOUR_CHAR_CODE('udng')
keyAECurrentPoint = FOUR_CHAR_CODE('cpos')
keyAEBufferSize = FOUR_CHAR_CODE('buff')
keyAEMoveView = FOUR_CHAR_CODE('mvvw')
keyAENextBody = FOUR_CHAR_CODE('nxbd')
keyAETSMScriptTag = FOUR_CHAR_CODE('sclg')
keyAETSMTextFont = FOUR_CHAR_CODE('ktxf')
keyAETSMTextFMFont = FOUR_CHAR_CODE('ktxm')
keyAETSMTextPointSize = FOUR_CHAR_CODE('ktps')
keyAETSMEventRecord = FOUR_CHAR_CODE('tevt')
keyAETSMEventRef = FOUR_CHAR_CODE('tevr')
keyAETextServiceEncoding = FOUR_CHAR_CODE('tsen')
keyAETextServiceMacEncoding = FOUR_CHAR_CODE('tmen')
typeTextRange = FOUR_CHAR_CODE('txrn')
typeComponentInstance = FOUR_CHAR_CODE('cmpi')
typeOffsetArray = FOUR_CHAR_CODE('ofay')
typeTextRangeArray = FOUR_CHAR_CODE('tray')
typeLowLevelEventRecord = FOUR_CHAR_CODE('evtr')
typeEventRef = FOUR_CHAR_CODE('evrf')
typeText = typeChar
kTSMOutsideOfBody = 1
kTSMInsideOfBody = 2
kTSMInsideOfActiveInputArea = 3
kNextBody = 1
kPreviousBody = 2
kCaretPosition = 1
kRawText = 2
kSelectedRawText = 3
kConvertedText = 4
kSelectedConvertedText = 5
kBlockFillText = 6
kOutlineText = 7
kSelectedText = 8
keyAEHiliteRange = FOUR_CHAR_CODE('hrng')
keyAEPinRange = FOUR_CHAR_CODE('pnrg')
keyAEClauseOffsets = FOUR_CHAR_CODE('clau')
keyAEOffset = FOUR_CHAR_CODE('ofst')
keyAEPoint = FOUR_CHAR_CODE('gpos')
keyAELeftSide = FOUR_CHAR_CODE('klef')
keyAERegionClass = FOUR_CHAR_CODE('rgnc')
keyAEDragging = FOUR_CHAR_CODE('bool')
keyAELeadingEdge = keyAELeftSide
typeUnicodeText = FOUR_CHAR_CODE('utxt')
typeStyledUnicodeText = FOUR_CHAR_CODE('sutx')
typeEncodedString = FOUR_CHAR_CODE('encs')
typeCString = FOUR_CHAR_CODE('cstr')
typePString = FOUR_CHAR_CODE('pstr')
typeMeters = FOUR_CHAR_CODE('metr')
typeInches = FOUR_CHAR_CODE('inch')
typeFeet = FOUR_CHAR_CODE('feet')
typeYards = FOUR_CHAR_CODE('yard')
typeMiles = FOUR_CHAR_CODE('mile')
typeKilometers = FOUR_CHAR_CODE('kmtr')
typeCentimeters = FOUR_CHAR_CODE('cmtr')
typeSquareMeters = FOUR_CHAR_CODE('sqrm')
typeSquareFeet = FOUR_CHAR_CODE('sqft')
typeSquareYards = FOUR_CHAR_CODE('sqyd')
typeSquareMiles = FOUR_CHAR_CODE('sqmi')
typeSquareKilometers = FOUR_CHAR_CODE('sqkm')
typeLiters = FOUR_CHAR_CODE('litr')
typeQuarts = FOUR_CHAR_CODE('qrts')
typeGallons = FOUR_CHAR_CODE('galn')
typeCubicMeters = FOUR_CHAR_CODE('cmet')
typeCubicFeet = FOUR_CHAR_CODE('cfet')
typeCubicInches = FOUR_CHAR_CODE('cuin')
typeCubicCentimeter = FOUR_CHAR_CODE('ccmt')
typeCubicYards = FOUR_CHAR_CODE('cyrd')
typeKilograms = FOUR_CHAR_CODE('kgrm')
typeGrams = FOUR_CHAR_CODE('gram')
typeOunces = FOUR_CHAR_CODE('ozs ')
typePounds = FOUR_CHAR_CODE('lbs ')
typeDegreesC = FOUR_CHAR_CODE('degc')
typeDegreesF = FOUR_CHAR_CODE('degf')
typeDegreesK = FOUR_CHAR_CODE('degk')
kFAServerApp = FOUR_CHAR_CODE('ssrv')
kDoFolderActionEvent = FOUR_CHAR_CODE('fola')
kFolderActionCode = FOUR_CHAR_CODE('actn')
kFolderOpenedEvent = FOUR_CHAR_CODE('fopn')
kFolderClosedEvent = FOUR_CHAR_CODE('fclo')
kFolderWindowMovedEvent = FOUR_CHAR_CODE('fsiz')
kFolderItemsAddedEvent = FOUR_CHAR_CODE('fget')
kFolderItemsRemovedEvent = FOUR_CHAR_CODE('flos')
kItemList = FOUR_CHAR_CODE('flst')
kNewSizeParameter = FOUR_CHAR_CODE('fnsz')
kFASuiteCode = FOUR_CHAR_CODE('faco')
kFAAttachCommand = FOUR_CHAR_CODE('atfa')
kFARemoveCommand = FOUR_CHAR_CODE('rmfa')
kFAEditCommand = FOUR_CHAR_CODE('edfa')
kFAFileParam = FOUR_CHAR_CODE('faal')
kFAIndexParam = FOUR_CHAR_CODE('indx')
kAEInternetSuite = FOUR_CHAR_CODE('gurl')
kAEISWebStarSuite = FOUR_CHAR_CODE('WWW\xbd')
kAEISGetURL = FOUR_CHAR_CODE('gurl')
KAEISHandleCGI = FOUR_CHAR_CODE('sdoc')
cURL = FOUR_CHAR_CODE('url ')
cInternetAddress = FOUR_CHAR_CODE('IPAD')
cHTML = FOUR_CHAR_CODE('html')
cFTPItem = FOUR_CHAR_CODE('ftp ')
kAEISHTTPSearchArgs = FOUR_CHAR_CODE('kfor')
kAEISPostArgs = FOUR_CHAR_CODE('post')
kAEISMethod = FOUR_CHAR_CODE('meth')
kAEISClientAddress = FOUR_CHAR_CODE('addr')
kAEISUserName = FOUR_CHAR_CODE('user')
kAEISPassword = FOUR_CHAR_CODE('pass')
kAEISFromUser = FOUR_CHAR_CODE('frmu')
kAEISServerName = FOUR_CHAR_CODE('svnm')
kAEISServerPort = FOUR_CHAR_CODE('svpt')
kAEISScriptName = FOUR_CHAR_CODE('scnm')
kAEISContentType = FOUR_CHAR_CODE('ctyp')
kAEISReferrer = FOUR_CHAR_CODE('refr')
kAEISUserAgent = FOUR_CHAR_CODE('Agnt')
kAEISAction = FOUR_CHAR_CODE('Kact')
kAEISActionPath = FOUR_CHAR_CODE('Kapt')
kAEISClientIP = FOUR_CHAR_CODE('Kcip')
kAEISFullRequest = FOUR_CHAR_CODE('Kfrq')
pScheme = FOUR_CHAR_CODE('pusc')
pHost = FOUR_CHAR_CODE('HOST')
pPath = FOUR_CHAR_CODE('FTPc')
pUserName = FOUR_CHAR_CODE('RAun')
pUserPassword = FOUR_CHAR_CODE('RApw')
pDNSForm = FOUR_CHAR_CODE('pDNS')
pURL = FOUR_CHAR_CODE('pURL')
pTextEncoding = FOUR_CHAR_CODE('ptxe')
pFTPKind = FOUR_CHAR_CODE('kind')
eScheme = FOUR_CHAR_CODE('esch')
eurlHTTP = FOUR_CHAR_CODE('http')
eurlHTTPS = FOUR_CHAR_CODE('htps')
eurlFTP = FOUR_CHAR_CODE('ftp ')
eurlMail = FOUR_CHAR_CODE('mail')
eurlFile = FOUR_CHAR_CODE('file')
eurlGopher = FOUR_CHAR_CODE('gphr')
eurlTelnet = FOUR_CHAR_CODE('tlnt')
eurlNews = FOUR_CHAR_CODE('news')
eurlSNews = FOUR_CHAR_CODE('snws')
eurlNNTP = FOUR_CHAR_CODE('nntp')
eurlMessage = FOUR_CHAR_CODE('mess')
eurlMailbox = FOUR_CHAR_CODE('mbox')
eurlMulti = FOUR_CHAR_CODE('mult')
eurlLaunch = FOUR_CHAR_CODE('laun')
eurlAFP = FOUR_CHAR_CODE('afp ')
eurlAT = FOUR_CHAR_CODE('at  ')
eurlEPPC = FOUR_CHAR_CODE('eppc')
eurlRTSP = FOUR_CHAR_CODE('rtsp')
eurlIMAP = FOUR_CHAR_CODE('imap')
eurlNFS = FOUR_CHAR_CODE('unfs')
eurlPOP = FOUR_CHAR_CODE('upop')
eurlLDAP = FOUR_CHAR_CODE('uldp')
eurlUnknown = FOUR_CHAR_CODE('url?')
kConnSuite = FOUR_CHAR_CODE('macc')
cDevSpec = FOUR_CHAR_CODE('cdev')
cAddressSpec = FOUR_CHAR_CODE('cadr')
cADBAddress = FOUR_CHAR_CODE('cadb')
cAppleTalkAddress = FOUR_CHAR_CODE('cat ')
cBusAddress = FOUR_CHAR_CODE('cbus')
cEthernetAddress = FOUR_CHAR_CODE('cen ')
cFireWireAddress = FOUR_CHAR_CODE('cfw ')
cIPAddress = FOUR_CHAR_CODE('cip ')
cLocalTalkAddress = FOUR_CHAR_CODE('clt ')
cSCSIAddress = FOUR_CHAR_CODE('cscs')
cTokenRingAddress = FOUR_CHAR_CODE('ctok')
cUSBAddress = FOUR_CHAR_CODE('cusb')
pDeviceType = FOUR_CHAR_CODE('pdvt')
pDeviceAddress = FOUR_CHAR_CODE('pdva')
pConduit = FOUR_CHAR_CODE('pcon')
pProtocol = FOUR_CHAR_CODE('pprt')
pATMachine = FOUR_CHAR_CODE('patm')
pATZone = FOUR_CHAR_CODE('patz')
pATType = FOUR_CHAR_CODE('patt')
pDottedDecimal = FOUR_CHAR_CODE('pipd')
pDNS = FOUR_CHAR_CODE('pdns')
pPort = FOUR_CHAR_CODE('ppor')
pNetwork = FOUR_CHAR_CODE('pnet')
pNode = FOUR_CHAR_CODE('pnod')
pSocket = FOUR_CHAR_CODE('psoc')
pSCSIBus = FOUR_CHAR_CODE('pscb')
pSCSILUN = FOUR_CHAR_CODE('pslu')
eDeviceType = FOUR_CHAR_CODE('edvt')
eAddressSpec = FOUR_CHAR_CODE('eads')
eConduit = FOUR_CHAR_CODE('econ')
eProtocol = FOUR_CHAR_CODE('epro')
eADB = FOUR_CHAR_CODE('eadb')
eAnalogAudio = FOUR_CHAR_CODE('epau')
eAppleTalk = FOUR_CHAR_CODE('epat')
eAudioLineIn = FOUR_CHAR_CODE('ecai')
eAudioLineOut = FOUR_CHAR_CODE('ecal')
eAudioOut = FOUR_CHAR_CODE('ecao')
eBus = FOUR_CHAR_CODE('ebus')
eCDROM = FOUR_CHAR_CODE('ecd ')
eCommSlot = FOUR_CHAR_CODE('eccm')
eDigitalAudio = FOUR_CHAR_CODE('epda')
eDisplay = FOUR_CHAR_CODE('edds')
eDVD = FOUR_CHAR_CODE('edvd')
eEthernet = FOUR_CHAR_CODE('ecen')
eFireWire = FOUR_CHAR_CODE('ecfw')
eFloppy = FOUR_CHAR_CODE('efd ')
eHD = FOUR_CHAR_CODE('ehd ')
eInfrared = FOUR_CHAR_CODE('ecir')
eIP = FOUR_CHAR_CODE('epip')
eIrDA = FOUR_CHAR_CODE('epir')
eIRTalk = FOUR_CHAR_CODE('epit')
eKeyboard = FOUR_CHAR_CODE('ekbd')
eLCD = FOUR_CHAR_CODE('edlc')
eLocalTalk = FOUR_CHAR_CODE('eclt')
eMacIP = FOUR_CHAR_CODE('epmi')
eMacVideo = FOUR_CHAR_CODE('epmv')
eMicrophone = FOUR_CHAR_CODE('ecmi')
eModemPort = FOUR_CHAR_CODE('ecmp')
eModemPrinterPort = FOUR_CHAR_CODE('empp')
eModem = FOUR_CHAR_CODE('edmm')
eMonitorOut = FOUR_CHAR_CODE('ecmn')
eMouse = FOUR_CHAR_CODE('emou')
eNuBusCard = FOUR_CHAR_CODE('ednb')
eNuBus = FOUR_CHAR_CODE('enub')
ePCcard = FOUR_CHAR_CODE('ecpc')
ePCIbus = FOUR_CHAR_CODE('ecpi')
ePCIcard = FOUR_CHAR_CODE('edpi')
ePDSslot = FOUR_CHAR_CODE('ecpd')
ePDScard = FOUR_CHAR_CODE('epds')
ePointingDevice = FOUR_CHAR_CODE('edpd')
ePostScript = FOUR_CHAR_CODE('epps')
ePPP = FOUR_CHAR_CODE('eppp')
ePrinterPort = FOUR_CHAR_CODE('ecpp')
ePrinter = FOUR_CHAR_CODE('edpr')
eSvideo = FOUR_CHAR_CODE('epsv')
eSCSI = FOUR_CHAR_CODE('ecsc')
eSerial = FOUR_CHAR_CODE('epsr')
eSpeakers = FOUR_CHAR_CODE('edsp')
eStorageDevice = FOUR_CHAR_CODE('edst')
eSVGA = FOUR_CHAR_CODE('epsg')
eTokenRing = FOUR_CHAR_CODE('etok')
eTrackball = FOUR_CHAR_CODE('etrk')
eTrackpad = FOUR_CHAR_CODE('edtp')
eUSB = FOUR_CHAR_CODE('ecus')
eVideoIn = FOUR_CHAR_CODE('ecvi')
eVideoMonitor = FOUR_CHAR_CODE('edvm')
eVideoOut = FOUR_CHAR_CODE('ecvo')
cKeystroke = FOUR_CHAR_CODE('kprs')
pKeystrokeKey = FOUR_CHAR_CODE('kMsg')
pModifiers = FOUR_CHAR_CODE('kMod')
pKeyKind = FOUR_CHAR_CODE('kknd')
eModifiers = FOUR_CHAR_CODE('eMds')
eOptionDown = FOUR_CHAR_CODE('Kopt')
eCommandDown = FOUR_CHAR_CODE('Kcmd')
eControlDown = FOUR_CHAR_CODE('Kctl')
eShiftDown = FOUR_CHAR_CODE('Ksft')
eCapsLockDown = FOUR_CHAR_CODE('Kclk')
eKeyKind = FOUR_CHAR_CODE('ekst')
eEscapeKey = 0x6B733500
eDeleteKey = 0x6B733300
eTabKey = 0x6B733000
eReturnKey = 0x6B732400
eClearKey = 0x6B734700
eEnterKey = 0x6B734C00
eUpArrowKey = 0x6B737E00
eDownArrowKey = 0x6B737D00
eLeftArrowKey = 0x6B737B00
eRightArrowKey = 0x6B737C00
eHelpKey = 0x6B737200
eHomeKey = 0x6B737300
ePageUpKey = 0x6B737400
ePageDownKey = 0x6B737900
eForwardDelKey = 0x6B737500
eEndKey = 0x6B737700
eF1Key = 0x6B737A00
eF2Key = 0x6B737800
eF3Key = 0x6B736300
eF4Key = 0x6B737600
eF5Key = 0x6B736000
eF6Key = 0x6B736100
eF7Key = 0x6B736200
eF8Key = 0x6B736400
eF9Key = 0x6B736500
eF10Key = 0x6B736D00
eF11Key = 0x6B736700
eF12Key = 0x6B736F00
eF13Key = 0x6B736900
eF14Key = 0x6B736B00
eF15Key = 0x6B737100
kAEAND = FOUR_CHAR_CODE('AND ')
kAEOR = FOUR_CHAR_CODE('OR  ')
kAENOT = FOUR_CHAR_CODE('NOT ')
kAEFirst = FOUR_CHAR_CODE('firs')
kAELast = FOUR_CHAR_CODE('last')
kAEMiddle = FOUR_CHAR_CODE('midd')
kAEAny = FOUR_CHAR_CODE('any ')
kAEAll = FOUR_CHAR_CODE('all ')
kAENext = FOUR_CHAR_CODE('next')
kAEPrevious = FOUR_CHAR_CODE('prev')
keyAECompOperator = FOUR_CHAR_CODE('relo')
keyAELogicalTerms = FOUR_CHAR_CODE('term')
keyAELogicalOperator = FOUR_CHAR_CODE('logc')
keyAEObject1 = FOUR_CHAR_CODE('obj1')
keyAEObject2 = FOUR_CHAR_CODE('obj2')
keyAEDesiredClass = FOUR_CHAR_CODE('want')
keyAEContainer = FOUR_CHAR_CODE('from')
keyAEKeyForm = FOUR_CHAR_CODE('form')
keyAEKeyData = FOUR_CHAR_CODE('seld')
keyAERangeStart = FOUR_CHAR_CODE('star')
keyAERangeStop = FOUR_CHAR_CODE('stop')
keyDisposeTokenProc = FOUR_CHAR_CODE('xtok')
keyAECompareProc = FOUR_CHAR_CODE('cmpr')
keyAECountProc = FOUR_CHAR_CODE('cont')
keyAEMarkTokenProc = FOUR_CHAR_CODE('mkid')
keyAEMarkProc = FOUR_CHAR_CODE('mark')
keyAEAdjustMarksProc = FOUR_CHAR_CODE('adjm')
keyAEGetErrDescProc = FOUR_CHAR_CODE('indc')
formAbsolutePosition = FOUR_CHAR_CODE('indx')
formRelativePosition = FOUR_CHAR_CODE('rele')
formTest = FOUR_CHAR_CODE('test')
formRange = FOUR_CHAR_CODE('rang')
formPropertyID = FOUR_CHAR_CODE('prop')
formName = FOUR_CHAR_CODE('name')
typeObjectSpecifier = FOUR_CHAR_CODE('obj ')
typeObjectBeingExamined = FOUR_CHAR_CODE('exmn')
typeCurrentContainer = FOUR_CHAR_CODE('ccnt')
typeToken = FOUR_CHAR_CODE('toke')
typeRelativeDescriptor = FOUR_CHAR_CODE('rel ')
typeAbsoluteOrdinal = FOUR_CHAR_CODE('abso')
typeIndexDescriptor = FOUR_CHAR_CODE('inde')
typeRangeDescriptor = FOUR_CHAR_CODE('rang')
typeLogicalDescriptor = FOUR_CHAR_CODE('logi')
typeCompDescriptor = FOUR_CHAR_CODE('cmpd')
typeOSLTokenList = FOUR_CHAR_CODE('ostl')
kAEIDoMinimum = 0x0000
kAEIDoWhose = 0x0001
kAEIDoMarking = 0x0004
kAEPassSubDescs = 0x0008
kAEResolveNestedLists = 0x0010
kAEHandleSimpleRanges = 0x0020
kAEUseRelativeIterators = 0x0040
typeWhoseDescriptor = FOUR_CHAR_CODE('whos')
formWhose = FOUR_CHAR_CODE('whos')
typeWhoseRange = FOUR_CHAR_CODE('wrng')
keyAEWhoseRangeStart = FOUR_CHAR_CODE('wstr')
keyAEWhoseRangeStop = FOUR_CHAR_CODE('wstp')
keyAEIndex = FOUR_CHAR_CODE('kidx')
keyAETest = FOUR_CHAR_CODE('ktst')
