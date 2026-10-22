#!/usr/bin/env python3
"""Prepare decoded 23.1.3 and merge its owned expansion assets without OBB.

No APK, vendor smali, signing keys or game assets belong in this repository.
Run prepare once on a fresh apktool decode; pack after apktool rebuild.
"""
import argparse
import hashlib
import json
import pathlib
import re
import shutil
import struct
import xml.etree.ElementTree as ET
import zipfile

ANDROID="{http://schemas.android.com/apk/res/android}"
PACKAGE="org.openpixelgun.opg2313"
TRACKERS=("com.google.","com.appsflyer.","com.applovin.","com.facebook.",
          "com.amazon.","com.chartboost.","com.tapjoy.","com.vungle.",
          "com.ironsource.","com.bytedance.","com.flurry.","com.mbridge.",
          "com.unity3d.ads.","com.unity3d.services.","com.unity3d.plugin.downloader.")

def smali(root, suffix):
 matches=list(root.glob("smali*/"+suffix))
 if len(matches)!=1:raise ValueError(f"expected one smali file: {suffix}")
 return matches[0]

def replace_method(path, signature, body):
 source=path.read_text()
 pattern=r"(?m)^\.method "+re.escape(signature)+r"\n.*?^\.end method"
 new=".method "+signature+"\n"+body.strip()+"\n.end method"
 source,count=re.subn(pattern,lambda _:new,source,flags=re.S)
 if count!=1:raise ValueError(f"missing/ambiguous method {signature}")
 path.write_text(source)

def callback(name, payload=""):
 return (f'    .locals 2\n    const-string v0, "{name}"\n'
         f'    const-string v1, "{payload}"\n'
         '    invoke-virtual {p0, v0, v1}, Lcom/prime31/GoogleIABPlugin;->UnitySendMessage(Ljava/lang/String;Ljava/lang/String;)V\n'
         '    return-void')

def apply_exact_patches(data, patches):
 # Validate every site before changing any bytes, including overlapping sites.
 spans=[]
 for offset, expected, replacement in patches:
  if len(expected)!=len(replacement) or not expected:
   raise ValueError('patch must preserve nonzero instruction span')
  if any(offset<end and offset+len(expected)>start for start,end in spans):
   raise ValueError('overlapping native patches')
  if offset<0 or data[offset:offset+len(expected)]!=expected:
   raise ValueError(f'native instruction signature mismatch at {offset:#x}')
  spans.append((offset,offset+len(expected)))
 for offset,expected,replacement in patches:
  data[offset:offset+len(expected)]=replacement

def embedded_native_patches():
 # Initializer's certificate query passes a literal com.pixel.gun3d to Android
 # PackageManager.getPackageInfo. In the separately installed APK this throws
 # NameNotFoundException before Photon connects. Use Application.identifier at
 # THIS call site only; leave protocol/JNI IDs and the real certificate query
 # untouched. x20 roots the args array; x21 holds the returned managed string
 # across the stock array element type check (both are callee-saved registers).
 call_site=0x2ED7CB4
 identifier=0x4441E10
 delta=identifier-call_site
 if delta%4 or not -(1<<27)<=delta<(1<<27):
  raise ValueError('Application.identifier call is outside AArch64 BL range')
 call=0x94000000|((delta//4)&0x03ffffff)
 words=lambda *values:struct.pack('<'+'I'*len(values),*values)
 return [
  (0x43740AC,bytes.fromhex('fa0f1bf8f96301a9'),bytes.fromhex('e0031f2ac0035fd6')),
  (0x2ED7CB0,words(0x9001c575,0xf94202b5,0xaa0003f4,0xf94002a1),
   words(0xaa0003f4,call,0xaa0003f5,0xaa0003e1)),
  (0x2ED7CDC,words(0xf94002a8),words(0xaa1503e8)),
 ]

def patch_embedded_assets(path):
 # Exact owned 23.1.3 input only. OBB coroutine success has no initialization
 # side effects; AppsMenu.Start continues with assets merged into the APK.
 # Never disable File.Exists, spoof a package, or suppress a signature error.
 data=bytearray(path.read_bytes())
 if hashlib.sha256(data).hexdigest()!='f0a130c4e8d9487059eab4b0f08462f6aa7d057510cada0fd6fb3043c77deb5c':
  raise ValueError('unsupported 23.1.3 engine binary')
 # Validate the direct getter's argument-independent exact-build entry too.
 if data[0x4441E10:0x4441E38]!=bytes.fromhex(
  'f37bbfa9534101f0601243f9a00000b5606d00f00020319118403997601203f9f37bc1a800001fd6'):
  raise ValueError('Application.identifier entry signature mismatch')
 apply_exact_patches(data,embedded_native_patches())
 path.write_bytes(data)

def prepare(root, loader):
 ET.register_namespace('android',ANDROID[1:-1])
 path=root/'AndroidManifest.xml';tree=ET.parse(path);manifest=tree.getroot()
 old=manifest.get('package');assert old=='com.pixel.gun3d'
 manifest.set('package',PACKAGE)
 permissions={'android.permission.INTERNET','android.permission.ACCESS_NETWORK_STATE',
              'android.permission.ACCESS_WIFI_STATE','android.permission.WAKE_LOCK','android.permission.VIBRATE',
              'android.permission.FOREGROUND_SERVICE'}
 removed=[]
 for node in list(manifest):
  if node.tag in ('permission','uses-permission') and node.get(ANDROID+'name') not in permissions:
   removed.append(node.get(ANDROID+'name'));manifest.remove(node)
  elif node.tag=='queries':manifest.remove(node)
 app=manifest.find('application');app.set(ANDROID+'allowBackup','false');app.set(ANDROID+'fullBackupContent','false')
 app.set(ANDROID+'usesCleartextTraffic','true')
 for node in list(app):
  name=node.get(ANDROID+'name','')
  if name.startswith(TRACKERS) or name=='com.pixelgun.PixelGunMessagingService' or (node.tag=='meta-data' and name.startswith(('firebase_','google_','com.tapjoy.'))):
   removed.append(name);app.remove(node);continue
  if node.tag in ('activity','service','receiver','provider') and name.startswith('.'):
   node.set(ANDROID+'name',old+name)  # Java/JNI class identity is not the package ID.
  for attr,value in list(node.attrib.items()):
   if old in value:node.set(attr,value.replace(old,PACKAGE))
  if node.tag=='activity':
   for filt in list(node.findall('intent-filter')):
    if any(a.get(ANDROID+'name')=='android.intent.action.VIEW' for a in filt.findall('action')):node.remove(filt)
 for key in ['firebase_data_collection_default_enabled','firebase_analytics_collection_enabled',
             'firebase_analytics_collection_deactivated','firebase_messaging_auto_init_enabled']:
  value='true' if key.endswith('deactivated') else 'false'
  ET.SubElement(app,'meta-data',{ANDROID+'name':key,ANDROID+'value':value})
 tree.write(path,encoding='utf-8',xml_declaration=True)
 for path in root.glob('res/values*/strings.xml'):
  tree=ET.parse(path);changed=False
  for node in tree.getroot().findall('string'):
   if node.get('name')=='app_name':node.text='OPG2313';changed=True
  if changed:tree.write(path,encoding='utf-8',xml_declaration=True)
 # Skip FCM token requests, while retaining Unity and Prime31 lifecycle plumbing.
 replace_method(smali(root,'com/pixelgun/MessagingAndPrime31UnityPlayerActivity.smali'),
  'protected onCreate(Landroid/os/Bundle;)V',
  '    .locals 0\n    invoke-super {p0, p1}, Lcom/prime31/UnityPlayerNativeActivity;->onCreate(Landroid/os/Bundle;)V\n    return-void')
 # Unity probes Play Core even with merged assets. Reuse its existing
 # API-missing branch (b remains null) instead of constructing Google's manager
 # and enabling removed extraction services. No pending asset-pack Task exists.
 replace_method(smali(root,'com/unity3d/player/PlayAssetDeliveryUnityWrapper.smali'),
  'private constructor <init>(Landroid/content/Context;)V',
  '    .locals 0\n    invoke-direct {p0}, Ljava/lang/Object;-><init>()V\n    return-void')
 # Optional GMS feature detection must report SERVICE_MISSING, not throw due
 # to the intentionally absent vendor manifest metadata (or contact GMS).
 for signature in ['public static isGooglePlayServicesAvailable(Landroid/content/Context;)I',
                   'public static isGooglePlayServicesAvailable(Landroid/content/Context;I)I']:
  replace_method(smali(root,'com/google/android/gms/common/GooglePlayServicesUtilLight.smali'),signature,
   '    .locals 1\n    const/4 v0, 0x1\n    return v0')
 # Complete sign-in as cancelled: no Google account UI, no unresolved Task.
 replace_method(smali(root,'com/google/games/bridge/SignInRequest.smali'),
  'public process(Lcom/google/games/bridge/HelperFragment;)V',
  '    .locals 1\n    iput-object p1, p0, Lcom/google/games/bridge/SignInRequest;->helperFragment:Lcom/google/games/bridge/HelperFragment;\n    const/16 v0, 0x10\n    invoke-virtual {p0, v0}, Lcom/google/games/bridge/SignInRequest;->setFailure(I)V\n    return-void')
 billing=smali(root,'com/prime31/GoogleIABPlugin.smali')
 replace_method(billing,'public init(Ljava/lang/String;)V',
  '    .locals 2\n    new-instance v0, Lcom/prime31/Inventory;\n    invoke-direct {v0}, Lcom/prime31/Inventory;-><init>()V\n    iput-object v0, p0, Lcom/prime31/GoogleIABPlugin;->inventory:Lcom/prime31/Inventory;\n    const-string v0, "billingNotSupported"\n    const-string v1, ""\n    invoke-virtual {p0, v0, v1}, Lcom/prime31/GoogleIABPlugin;->UnitySendMessage(Ljava/lang/String;Ljava/lang/String;)V\n    return-void')
 for signature,event in [('public queryInventory([Ljava/lang/String;)V','queryInventoryFailed'),
                         ('public queryInventory([Ljava/lang/Object;)V','queryInventoryFailed'),
                         ('public consumeProduct(Ljava/lang/String;)V','consumePurchaseFailed'),
                         ('public acknowledgePurchase(Ljava/lang/String;)V','acknowledgePurchaseFailed')]:
  replace_method(billing,signature,callback(event))
 replace_method(billing,'public purchaseProduct(Ljava/lang/String;)V',callback('purchaseFailed',r'{\"response\":3,\"result\":\"\"}'))
 replace_method(billing,'public areSubscriptionsSupported()Z','    .locals 1\n    const/4 v0, 0x0\n    return v0')
 replace_method(billing,'public unbindService()V','    .locals 0\n    return-void')
 replace_method(billing,'public getPurchaseHistory()Ljava/lang/String;','    .locals 1\n    const-string v0, "[]"\n    return-object v0')
 analytics=smali(root,'com/google/firebase/analytics/FirebaseAnalytics.smali')
 signatures=re.findall(r'^\.method (public [^\n]+\)V)$',analytics.read_text(),re.M)
 for signature in signatures:
  if '<init>' not in signature:replace_method(analytics,signature,'    .locals 0\n    return-void')
 replace_method(analytics,'public static getInstance(Landroid/content/Context;)Lcom/google/firebase/analytics/FirebaseAnalytics;',
  '    .locals 2\n    const/4 v0, 0x0\n    new-instance v1, Lcom/google/firebase/analytics/FirebaseAnalytics;\n    invoke-direct {v1, v0}, Lcom/google/firebase/analytics/FirebaseAnalytics;-><init>(Lcom/google/android/gms/internal/measurement/zzef;)V\n    return-object v1')
 replace_method(analytics,'public static getScionFrontendApiImplementation(Landroid/content/Context;Landroid/os/Bundle;)Lcom/google/android/gms/measurement/internal/zzhy;',
  '    .locals 1\n    const/4 v0, 0x0\n    return-object v0')
 for signature in ['public getAppInstanceId()Lcom/google/android/gms/tasks/Task;','public getSessionId()Lcom/google/android/gms/tasks/Task;']:
  # Completed empty identifier, not a pending background collection request.
  replace_method(analytics,signature,'    .locals 1\n    const/4 v0, 0x0\n    invoke-static {v0}, Lcom/google/android/gms/tasks/Tasks;->forResult(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;\n    move-result-object v0\n    return-object v0')
 replace_method(analytics,'public getFirebaseInstanceId()Ljava/lang/String;','    .locals 1\n    const-string v0, ""\n    return-object v0')
 replace_method(smali(root,'com/google/firebase/FirebaseApp.smali'),'public isDataCollectionDefaultEnabled()Z','    .locals 1\n    const/4 v0, 0x0\n    return v0')
 native=root/'lib/arm64-v8a';shutil.copy2(loader,native/'libopg3d.so')
 patch_embedded_assets(native/'libil2cpp.so')
 # Package-ID strings in Android resources only; never rename Java descriptors.
 for path in (root/'res').rglob('*.xml'):
  source=path.read_text()
  if old in source:path.write_text(source.replace(old,PACKAGE))
 (root/'standalone-report.json').write_text(json.dumps({'package':PACKAGE,'label':'OPG2313','removed':removed,'mode':'merged-assets-no-obb'},indent=2))
 print('Prepared standalone manifest/lifecycle/Google terminal callbacks; removed',len(removed),'registrations')

def safe_name(name):
 p=pathlib.PurePosixPath(name)
 return not p.is_absolute() and '..' not in p.parts and '\\' not in name

def pack(base, obb, output):
 names=set();output.parent.mkdir(parents=True,exist_ok=True)
 temp=output.with_suffix('.tmp.apk')
 try:
  with zipfile.ZipFile(temp,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=False) as dest:
   for archive in [base,obb]:
    with zipfile.ZipFile(archive) as source:
     for info in source.infolist():
      name=info.filename
      if name=='unity_obb_guid' or name.startswith('META-INF/') or name=='standalone-report.json':continue
      if info.is_dir():continue
      if not safe_name(name) or name in names:raise ValueError('unsafe or duplicate path '+name)
      if archive==obb and not name.startswith('assets/'):raise ValueError('unexpected OBB file '+name)
      names.add(name)
      # Preserve original per-file compression; Unity already reads these from
      # ZIP in APK/OBB. Native .so entries remain stored for page alignment.
      target=zipfile.ZipInfo(name,info.date_time);target.compress_type=zipfile.ZIP_STORED if name.startswith('lib/') else info.compress_type
      target.external_attr=info.external_attr
      with source.open(info) as src,dest.open(target,'w') as out:shutil.copyfileobj(src,out,1024*1024)
  temp.replace(output)
 except Exception:
  temp.unlink(missing_ok=True);raise
 print('Merged',len(names),'entries; standalone bytes',output.stat().st_size)

def main():
 p=argparse.ArgumentParser();sub=p.add_subparsers(dest='command',required=True)
 a=sub.add_parser('prepare');a.add_argument('--decoded',type=pathlib.Path,required=True);a.add_argument('--loader',type=pathlib.Path,required=True)
 a=sub.add_parser('pack');a.add_argument('--base',type=pathlib.Path,required=True);a.add_argument('--obb',type=pathlib.Path,required=True);a.add_argument('--output',type=pathlib.Path,required=True)
 a=p.parse_args()
 if a.command=='prepare':prepare(a.decoded,a.loader)
 else:pack(a.base,a.obb,a.output)
if __name__=='__main__':main()
