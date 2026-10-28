import importlib.util
from pathlib import Path
import unittest

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('make_site', root/'web/make_site.py')
site = importlib.util.module_from_spec(spec)
spec.loader.exec_module(site)

class InstallerChannels(unittest.TestCase):
    def test_public_add_first_sample_retained(self):
        entries = [{'version':'v0.9.9-par13b-3641c02-preview','edition':'SAMPLE','status':'public'},
                   {'version':'v0.9.9-par10-30f05b3','edition':'ADDITIVE','status':'public'}]
        result = site.installable_versions(entries, False)
        self.assertEqual([e['edition'] for e in result], ['ADDITIVE','SAMPLE'])
        self.assertEqual(result[1], entries[0])

    def test_all_par14_variants_blocked_even_if_mislabeled(self):
        add = {'version':'v0.9.9-par10d-beta2-42947e9-preview','edition':'ADDITIVE + CONTROLS','status':'beta'}
        for suffix in ['beta1-86b85ad-preview','beta2-4be8592-preview','beta4-4d3156d-preview','future-1234567']:
            bad = {'version':'v0.9.9-par14-'+suffix,'edition':'SAMPLE','status':'public'}
            self.assertEqual(site.installable_versions([bad,add], True), [add])

    def test_withdrawn_only_fails_closed(self):
        with self.assertRaises(SystemExit):
            site.installable_versions([{'version':'v0.9.9-par14-beta4','edition':'SAMPLE'}],True)

    def test_beta_still_forbidden_in_public(self):
        with self.assertRaises(SystemExit):
            site.installable_versions([{'version':'v0.9.9-par10d-beta2','edition':'ADDITIVE','status':'beta'}],False)

if __name__ == '__main__':
    unittest.main()
