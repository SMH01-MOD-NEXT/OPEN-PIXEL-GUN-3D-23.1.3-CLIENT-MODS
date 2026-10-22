#!/usr/bin/env python3
import importlib.util
import pathlib
import struct
import tempfile
import unittest
import zipfile

spec=importlib.util.spec_from_file_location('standalone',pathlib.Path(__file__).with_name('standalone_apk.py'))
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class StandaloneTests(unittest.TestCase):
 def test_rejects_zip_traversal(self):
  for name in ['../outside','/root','assets/../bad','assets\\bad']:
   self.assertFalse(module.safe_name(name),name)
  self.assertTrue(module.safe_name('assets/bin/Data/level1'))

 def test_merges_assets_without_obb_or_signature(self):
  with tempfile.TemporaryDirectory() as t:
   root=pathlib.Path(t);base=root/'base.apk';obb=root/'main.obb';out=root/'out.apk'
   with zipfile.ZipFile(base,'w') as z:
    z.writestr('AndroidManifest.xml',b'manifest');z.writestr('META-INF/CERT.RSA',b'old');z.writestr('assets/bin/Data/level0',b'first')
   with zipfile.ZipFile(obb,'w') as z:
    z.writestr('unity_obb_guid',b'guid');z.writestr('assets/bin/Data/level1',b'second')
   module.pack(base,obb,out)
   with zipfile.ZipFile(out) as z:
    self.assertEqual(set(z.namelist()),{'AndroidManifest.xml','assets/bin/Data/level0','assets/bin/Data/level1'})
    self.assertEqual(z.read('assets/bin/Data/level1'),b'second')
   with zipfile.ZipFile(obb,'a') as z:z.writestr('assets/bin/Data/level0',b'collision')
   with self.assertRaises(ValueError):module.pack(base,obb,out)
   self.assertFalse(out.with_suffix('.tmp.apk').exists())

 def test_unknown_native_binary_is_not_modified(self):
  with tempfile.TemporaryDirectory() as t:
   p=pathlib.Path(t)/'libil2cpp.so';p.write_bytes(b'unsupported')
   with self.assertRaises(ValueError):module.patch_embedded_assets(p)
   self.assertEqual(p.read_bytes(),b'unsupported')

 def test_patch_validation_is_atomic(self):
  data=bytearray(b'abcdefgh')
  with self.assertRaises(ValueError):
   module.apply_exact_patches(data,[(0,b'ab',b'AB'),(5,b'XX',b'FG')])
  self.assertEqual(data,b'abcdefgh')
  for patches in [[(0,b'abc',b'ABC'),(2,b'cd',b'CD')],[(0,b'ab',b'A')],
                  [(-1,b'h',b'H')]]:
   with self.assertRaises(ValueError):module.apply_exact_patches(data,patches)
   self.assertEqual(data,b'abcdefgh')
  module.apply_exact_patches(data,[(0,b'ab',b'AB'),(5,b'fg',b'FG')])
  self.assertEqual(data,b'ABcdeFGh')

 def test_package_lookup_calls_real_application_identifier(self):
  patches=module.embedded_native_patches()
  self.assertEqual(len(patches),3)
  site,expected,replacement=patches[1]
  words=struct.unpack('<4I',replacement)
  self.assertEqual(site,0x2ED7CB0)
  self.assertEqual(words[0],0xaa0003f4) # preserve args in x20 before getter
  self.assertEqual(words[1]&0xfc000000,0x94000000) # BL, not a bypass branch
  imm=words[1]&0x03ffffff
  if imm&0x02000000:imm-=0x04000000
  self.assertEqual(site+4+4*imm,0x4441E10)
  self.assertEqual(words[2:],(0xaa0003f5,0xaa0003e1)) # string in x21/x1
  self.assertEqual(patches[2],(0x2ED7CDC,struct.pack('<I',0xf94002a8),struct.pack('<I',0xaa1503e8)))

 @unittest.skipUnless((pathlib.Path(__file__).resolve().parent.parent/'analys2313/libil2cpp.so').is_file(),
                      'owned exact-build ELF is not available')
 def test_owned_native_binary_changes_only_validated_sites(self):
  original=(pathlib.Path(__file__).resolve().parent.parent/'analys2313/libil2cpp.so').read_bytes()
  with tempfile.TemporaryDirectory() as t:
   p=pathlib.Path(t)/'libil2cpp.so';p.write_bytes(original)
   module.patch_embedded_assets(p)
   patched=p.read_bytes();self.assertEqual(len(original),len(patched))
   cursor=0
   for offset,expected,replacement in sorted(module.embedded_native_patches()):
    self.assertEqual(original[offset:offset+len(expected)],expected)
    self.assertEqual(patched[offset:offset+len(replacement)],replacement)
    self.assertEqual(original[cursor:offset],patched[cursor:offset])
    cursor=offset+len(expected)
   self.assertEqual(original[cursor:],patched[cursor:])
   with self.assertRaises(ValueError):module.patch_embedded_assets(p)
   self.assertEqual(p.read_bytes(),patched)

 def test_smali_target_must_be_exactly_once(self):
  with tempfile.TemporaryDirectory() as t:
   p=pathlib.Path(t)/'x.smali';p.write_text('.method public f()V\n .locals 0\n return-void\n.end method\n')
   module.replace_method(p,'public f()V',' .locals 0\n return-void')
   self.assertIn('.method public f()V',p.read_text())
   with self.assertRaises(ValueError):module.replace_method(p,'public missing()V','return-void')

if __name__=='__main__':unittest.main()
