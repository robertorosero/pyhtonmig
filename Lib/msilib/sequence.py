AdminExecuteSequence = [
(u'InstallInitialize', None, 1500),
(u'InstallFinalize', None, 6600),
(u'InstallFiles', None, 4000),
(u'InstallAdminPackage', None, 3900),
(u'FileCost', None, 900),
(u'CostInitialize', None, 800),
(u'CostFinalize', None, 1000),
(u'InstallValidate', None, 1400),
]

AdminUISequence = [
(u'FileCost', None, 900),
(u'CostInitialize', None, 800),
(u'CostFinalize', None, 1000),
(u'ExecuteAction', None, 1300),
(u'ExitDialog', None, -1),
(u'FatalError', None, -3),
(u'UserExit', None, -2),
]

AdvtExecuteSequence = [
(u'InstallInitialize', None, 1500),
(u'InstallFinalize', None, 6600),
(u'CostInitialize', None, 800),
(u'CostFinalize', None, 1000),
(u'InstallValidate', None, 1400),
(u'CreateShortcuts', None, 4500),
(u'MsiPublishAssemblies', None, 6250),
(u'PublishComponents', None, 6200),
(u'PublishFeatures', None, 6300),
(u'PublishProduct', None, 6400),
(u'RegisterClassInfo', None, 4600),
(u'RegisterExtensionInfo', None, 4700),
(u'RegisterMIMEInfo', None, 4900),
(u'RegisterProgIdInfo', None, 4800),
]

InstallExecuteSequence = [
(u'InstallInitialize', None, 1500),
(u'InstallFinalize', None, 6600),
(u'InstallFiles', None, 4000),
(u'FileCost', None, 900),
(u'CostInitialize', None, 800),
(u'CostFinalize', None, 1000),
(u'InstallValidate', None, 1400),
(u'CreateShortcuts', None, 4500),
(u'MsiPublishAssemblies', None, 6250),
(u'PublishComponents', None, 6200),
(u'PublishFeatures', None, 6300),
(u'PublishProduct', None, 6400),
(u'RegisterClassInfo', None, 4600),
(u'RegisterExtensionInfo', None, 4700),
(u'RegisterMIMEInfo', None, 4900),
(u'RegisterProgIdInfo', None, 4800),
(u'AllocateRegistrySpace', u'NOT Installed', 1550),
(u'AppSearch', None, 400),
(u'BindImage', None, 4300),
(u'CCPSearch', u'NOT Installed', 500),
(u'CreateFolders', None, 3700),
(u'DeleteServices', u'VersionNT', 2000),
(u'DuplicateFiles', None, 4210),
(u'FindRelatedProducts', None, 200),
(u'InstallODBC', None, 5400),
(u'InstallServices', u'VersionNT', 5800),
(u'IsolateComponents', None, 950),
(u'LaunchConditions', None, 100),
(u'MigrateFeatureStates', None, 1200),
(u'MoveFiles', None, 3800),
(u'PatchFiles', None, 4090),
(u'ProcessComponents', None, 1600),
(u'RegisterComPlus', None, 5700),
(u'RegisterFonts', None, 5300),
(u'RegisterProduct', None, 6100),
(u'RegisterTypeLibraries', None, 5500),
(u'RegisterUser', None, 6000),
(u'RemoveDuplicateFiles', None, 3400),
(u'RemoveEnvironmentStrings', None, 3300),
(u'RemoveExistingProducts', None, 6700),
(u'RemoveFiles', None, 3500),
(u'RemoveFolders', None, 3600),
(u'RemoveIniValues', None, 3100),
(u'RemoveODBC', None, 2400),
(u'RemoveRegistryValues', None, 2600),
(u'RemoveShortcuts', None, 3200),
(u'RMCCPSearch', u'NOT Installed', 600),
(u'SelfRegModules', None, 5600),
(u'SelfUnregModules', None, 2200),
(u'SetODBCFolders', None, 1100),
(u'StartServices', u'VersionNT', 5900),
(u'StopServices', u'VersionNT', 1900),
(u'MsiUnpublishAssemblies', None, 1750),
(u'UnpublishComponents', None, 1700),
(u'UnpublishFeatures', None, 1800),
(u'UnregisterClassInfo', None, 2700),
(u'UnregisterComPlus', None, 2100),
(u'UnregisterExtensionInfo', None, 2800),
(u'UnregisterFonts', None, 2500),
(u'UnregisterMIMEInfo', None, 3000),
(u'UnregisterProgIdInfo', None, 2900),
(u'UnregisterTypeLibraries', None, 2300),
(u'ValidateProductID', None, 700),
(u'WriteEnvironmentStrings', None, 5200),
(u'WriteIniValues', None, 5100),
(u'WriteRegistryValues', None, 5000),
]

InstallUISequence = [
(u'FileCost', None, 900),
(u'CostInitialize', None, 800),
(u'CostFinalize', None, 1000),
(u'ExecuteAction', None, 1300),
(u'ExitDialog', None, -1),
(u'FatalError', None, -3),
(u'UserExit', None, -2),
(u'AppSearch', None, 400),
(u'CCPSearch', u'NOT Installed', 500),
(u'FindRelatedProducts', None, 200),
(u'IsolateComponents', None, 950),
(u'LaunchConditions', None, 100),
(u'MigrateFeatureStates', None, 1200),
(u'RMCCPSearch', u'NOT Installed', 600),
(u'ValidateProductID', None, 700),
]

tables=['AdminExecuteSequence', 'AdminUISequence', 'AdvtExecuteSequence', 'InstallExecuteSequence', 'InstallUISequence']
