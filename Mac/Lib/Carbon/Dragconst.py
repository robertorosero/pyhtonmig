# Generated from 'Drag.h'

def FOUR_CHAR_CODE(x): return x
from TextEdit import *
from QuickDraw import *


kDragHasLeftSenderWindow	= (1 << 0)
kDragInsideSenderApplication = (1 << 1)
kDragInsideSenderWindow		= (1 << 2)
kDragRegionAndImage			= (1 << 4)
flavorSenderOnly			= (1 << 0)
flavorSenderTranslated		= (1 << 1)
flavorNotSaved				= (1 << 2)
flavorSystemTranslated		= (1 << 8)
kDragHasLeftSenderWindow = (1L << 0)
kDragInsideSenderApplication = (1L << 1)
kDragInsideSenderWindow = (1L << 2) 
kDragBehaviorNone = 0
kDragBehaviorZoomBackAnimation = (1L << 0) 
kDragRegionAndImage = (1L << 4) 
kDragStandardTranslucency = 0L
kDragDarkTranslucency = 1L
kDragDarkerTranslucency = 2L
kDragOpaqueTranslucency = 3L    
kDragRegionBegin = 1
kDragRegionDraw = 2
kDragRegionHide = 3
kDragRegionIdle = 4
kDragRegionEnd = 5     
kZoomNoAcceleration = 0
kZoomAccelerate = 1
kZoomDecelerate = 2     
flavorSenderOnly = (1 << 0)
flavorSenderTranslated = (1 << 1)
flavorNotSaved = (1 << 2)
flavorSystemTranslated = (1 << 8) 
kDragFlavorTypeHFS = FOUR_CHAR_CODE('hfs ')
kDragFlavorTypePromiseHFS = FOUR_CHAR_CODE('phfs')
flavorTypeHFS = kDragFlavorTypeHFS
flavorTypePromiseHFS = kDragFlavorTypePromiseHFS 
kDragPromisedFlavorFindFile = FOUR_CHAR_CODE('rWm1')
kDragPromisedFlavor = FOUR_CHAR_CODE('fssP') 
kDragPseudoCreatorVolumeOrDirectory = FOUR_CHAR_CODE('MACS')
kDragPseudoFileTypeVolume = FOUR_CHAR_CODE('disk')
kDragPseudoFileTypeDirectory = FOUR_CHAR_CODE('fold') 
flavorTypeDirectory = FOUR_CHAR_CODE('diry') 
kFlavorTypeClippingName = FOUR_CHAR_CODE('clnm')
kFlavorTypeClippingFilename = FOUR_CHAR_CODE('clfn')
kFlavorTypeDragToTrashOnly = FOUR_CHAR_CODE('fdtt')
kFlavorTypeFinderNoTrackingBehavior = FOUR_CHAR_CODE('fntb') 
kDragTrackingEnterHandler = 1
kDragTrackingEnterWindow = 2
kDragTrackingInWindow = 3
kDragTrackingLeaveWindow = 4
kDragTrackingLeaveHandler = 5     
dragHasLeftSenderWindow = kDragHasLeftSenderWindow
dragInsideSenderApplication = kDragInsideSenderApplication
dragInsideSenderWindow = kDragInsideSenderWindow 
dragTrackingEnterHandler = kDragTrackingEnterHandler
dragTrackingEnterWindow = kDragTrackingEnterWindow
dragTrackingInWindow = kDragTrackingInWindow
dragTrackingLeaveWindow = kDragTrackingLeaveWindow
dragTrackingLeaveHandler = kDragTrackingLeaveHandler 
dragRegionBegin = kDragRegionBegin
dragRegionDraw = kDragRegionDraw
dragRegionHide = kDragRegionHide
dragRegionIdle = kDragRegionIdle
dragRegionEnd = kDragRegionEnd 
zoomNoAcceleration = kZoomNoAcceleration
zoomAccelerate = kZoomAccelerate
zoomDecelerate = kZoomDecelerate 
kDragStandardImage = kDragStandardTranslucency
kDragDarkImage = kDragDarkTranslucency
kDragDarkerImage = kDragDarkerTranslucency
kDragOpaqueImage = kDragOpaqueTranslucency 
