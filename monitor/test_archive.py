import json
import tempfile
import unittest
from pathlib import Path
from archive import Archive


def record(i, category=1):
    return {'id':i,'chain':1,'category':category,'game_second':i,'action':'finished','actor':'robot'}


def memory(items, latest=None):
    return {'enabled':True,'world_id':'normal-test','records':items,'long_term':[],
            'latest_id':latest if latest is not None else max(x['id'] for x in items)}


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.path=Path(self.temp.name)/'archive.sqlite3';self.archive=Archive(self.path)

    def tearDown(self): self.temp.cleanup()

    def test_dedupe_reboot_and_permanent_disk(self):
        self.archive.ingest(memory([record(1),record(2,2)]))
        self.archive.ingest(memory([record(1),record(2,2)]))
        reopened=Archive(self.path)
        self.assertEqual(reopened.summary('normal-test')['events'],2)
        self.assertEqual(reopened.summary('normal-test')['missing_details'],0)
        self.assertEqual(reopened.query('normal-test',category=2)['records'][0]['id'],2)

    def test_gaps_and_backfill(self):
        self.archive.ingest(memory([record(4),record(5)]))
        self.assertEqual(self.archive.summary('normal-test')['missing_details'],3)
        self.archive.ingest(memory([record(1),record(2),record(3)]))
        self.assertEqual(self.archive.summary('normal-test')['missing_details'],0)

    def test_conflict_atomic_rollback(self):
        self.archive.ingest(memory([record(1)]))
        changed=record(1);changed['actor']='player'
        with self.assertRaises(ValueError): self.archive.ingest(memory([record(2),changed]))
        self.assertEqual(self.archive.summary('normal-test')['events'],1)
        self.assertEqual(len(self.archive.query('normal-test')['records']),1)

    def test_world_isolation_and_paging(self):
        self.archive.ingest(memory([record(i) for i in range(1,10)]))
        other=memory([record(1)]);other['world_id']='demo-test';self.archive.ingest(other)
        page=self.archive.query('normal-test',limit=3)
        self.assertEqual([x['id'] for x in page['records']],[9,8,7])
        page2=self.archive.query('normal-test',before=page['next_before'],limit=3)
        self.assertEqual([x['id'] for x in page2['records']],[6,5,4])
        self.assertEqual(self.archive.summary('demo-test')['events'],1)

    def test_milestones_survive_ring_rollover(self):
        first=memory([record(1)]);first['long_term']=[{'type':20,'evidence_id':1}]
        self.archive.ingest(first)
        self.archive.ingest(memory([record(i) for i in range(100,228)]))
        self.assertEqual(self.archive.query('normal-test')['long_term'],first['long_term'])


if __name__=='__main__': unittest.main()
