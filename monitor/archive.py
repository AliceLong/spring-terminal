"""Append-only local evidence archive. No device commands, no model summaries."""
from contextlib import contextmanager
import json
import os
import sqlite3
import threading
import time
from pathlib import Path


class Archive:
    def __init__(self, path):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        self.path.touch(mode=0o600, exist_ok=True)
        self.path.chmod(0o600)
        self.lock = threading.Lock()
        with self.db() as c:
            c.executescript('''
                CREATE TABLE IF NOT EXISTS experience(
                    world TEXT NOT NULL, id INTEGER NOT NULL, category INTEGER NOT NULL,
                    chain INTEGER NOT NULL, game_second INTEGER NOT NULL,
                    payload TEXT NOT NULL, received REAL NOT NULL, PRIMARY KEY(world,id));
                CREATE INDEX IF NOT EXISTS experience_topic ON experience(world,category,id);
                CREATE INDEX IF NOT EXISTS experience_chain ON experience(world,chain,id);
                CREATE TABLE IF NOT EXISTS fact(
                    world TEXT NOT NULL, type INTEGER NOT NULL, payload TEXT NOT NULL,
                    PRIMARY KEY(world,type));
                CREATE TABLE IF NOT EXISTS coverage(
                    world TEXT PRIMARY KEY, maximum INTEGER NOT NULL, received INTEGER NOT NULL);
            ''')

    @contextmanager
    def db(self):
        c = sqlite3.connect(self.path, timeout=5)
        try:
            c.execute('PRAGMA journal_mode=WAL')
            with c:
                yield c
        finally:
            c.close()

    def ingest(self, memory):
        if not memory or not memory.get('enabled'):
            return
        world = memory.get('world_id')
        if not isinstance(world, str) or not 1 <= len(world) <= 64:
            raise ValueError('invalid world identity')
        records, facts = memory.get('records', []), memory.get('long_term', [])
        if len(records) > 128 or len(facts) > 32:
            raise ValueError('invalid archive bounds')
        with self.lock, self.db() as c:
            added = 0
            for record in records:
                ident, chain, category = record['id'], record['chain'], record['category']
                if (not isinstance(ident, int) or not 0 < chain <= ident
                        or not 1 <= category <= 5):
                    raise ValueError('invalid event')
                payload = json.dumps(record, ensure_ascii=False, sort_keys=True)
                old = c.execute('SELECT payload FROM experience WHERE world=? AND id=?', (world, ident)).fetchone()
                if old is not None and old[0] != payload:
                    raise ValueError('conflicting immutable event')
                cur = c.execute('INSERT OR IGNORE INTO experience VALUES(?,?,?,?,?,?,?)',
                    (world, ident, category, chain, record['game_second'], payload, time.time()))
                added += cur.rowcount
            for fact in facts:
                payload = json.dumps(fact, ensure_ascii=False, sort_keys=True)
                old = c.execute('SELECT payload FROM fact WHERE world=? AND type=?', (world, fact['type'])).fetchone()
                if old is not None and old[0] != payload:
                    raise ValueError('conflicting permanent evidence')
                c.execute('INSERT OR IGNORE INTO fact VALUES(?,?,?)', (world, fact['type'], payload))
            maximum = max([memory.get('latest_id', 0)] + [x['id'] for x in records])
            c.execute('''INSERT INTO coverage VALUES(?,?,?) ON CONFLICT(world) DO UPDATE SET
                maximum=max(maximum,excluded.maximum),received=received+excluded.received''',
                (world, maximum, added))

    def summary(self, world):
        if not world:
            return {'available': False}
        with self.lock, self.db() as c:
            row = c.execute('SELECT maximum,received FROM coverage WHERE world=?', (world,)).fetchone()
            facts = c.execute('SELECT count(*) FROM fact WHERE world=?', (world,)).fetchone()[0]
        maximum, received = row if row else (0, 0)
        return {'available': True, 'world_id': world, 'events': received,
                'latest_id': maximum, 'missing_details': maximum-received, 'long_term': facts}

    def query(self, world, category=0, chain=0, before=0, limit=40):
        clauses, args = ['world=?'], [world]
        if category:
            clauses.append('category=?');args.append(category)
        if chain:
            clauses.append('chain=?');args.append(chain)
        if before:
            clauses.append('id<?');args.append(before)
        args.append(limit)
        with self.lock, self.db() as c:
            rows = c.execute('SELECT payload FROM experience WHERE '+' AND '.join(clauses)+' ORDER BY id DESC LIMIT ?', args).fetchall()
            facts = c.execute('SELECT payload FROM fact WHERE world=? ORDER BY type', (world,)).fetchall()
        result = [json.loads(x[0]) for x in rows]
        return {'records': result, 'long_term': [json.loads(x[0]) for x in facts],
                'next_before': result[-1]['id'] if len(result) == limit else 0,
                'summary': self.summary(world)}
