const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const script = fs.readFileSync(path.join(__dirname, 'roadshow.js'), 'utf8');
const start = script.indexOf('async function readControlReply(');
const end = script.indexOf('\nfunction rows(', start);
assert(start >= 0 && end > start);
const parse = vm.runInNewContext(script.slice(start, end) + '\nreadControlReply');
const reply = (status, body) => ({status, ok: status >= 200 && status < 300, text: async () => body});
(async () => {
  assert.equal((await parse(reply(202, '{"phase":"QUEUED"}'))).phase, 'QUEUED');
  await assert.rejects(parse(reply(405, '<!DOCTYPE HTML><html>Read-only roadshow</html>')), /旧版/);
  await assert.rejects(parse(reply(503, '<!DOCTYPE HTML>')), /待设备确认/);
  await assert.rejects(parse(reply(409, '{"error":"正在通话"}')), /正在通话/);
  for (const body of ['null', '[]', 'false'])
    await assert.rejects(parse(reply(200, body)), /格式异常/);
  console.log('Roadshow reply tests passed: old HTML service, unavailable service, JSON rejection and acknowledgement.');
})().catch(error => { console.error(error); process.exitCode = 1; });
