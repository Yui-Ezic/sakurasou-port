"""One call to the local `claude` CLI with a JSON schema. Shared by tl_draft.py and tl_context.py.
Runs in an empty directory with no tools and no project settings, so the request holds only what we send."""
import datetime, json, os, re, subprocess, tempfile
from tl_common import ROOT


def ask(model, system, prompt, schema, timeout=1500):
    """Returns (structured output, cost in USD). Usage is appended to work/tl/usage.jsonl."""
    with tempfile.TemporaryDirectory() as d:
        sp = os.path.join(d, 'system.md')
        open(sp, 'w', encoding='utf8').write(system)
        r = subprocess.run(['claude', '-p', '--model', model, '--output-format', 'json', '--tools', '',
                            '--system-prompt-file', sp, '--setting-sources', '', '--no-session-persistence',
                            '--json-schema', json.dumps(schema)],
                           input=prompt, capture_output=True, text=True, cwd=d, timeout=timeout)
    if r.returncode:
        raise SystemExit(f'claude failed: {r.stdout[:400]} {r.stderr[:400]}')
    o = json.loads(r.stdout)
    os.makedirs(os.path.join(ROOT, 'work', 'tl'), exist_ok=True)
    with open(os.path.join(ROOT, 'work', 'tl', 'usage.jsonl'), 'a') as f:
        f.write(json.dumps(dict(model=model, cost=o.get('total_cost_usd'), usage=o.get('usage'), ms=o.get('duration_ms'),
                                time=datetime.datetime.now().isoformat())) + '\n')
    so = o.get('structured_output')
    if so is None:
        m = re.search(r'\{.*\}', o.get('result', ''), re.S)
        so = json.loads(m.group())
    return so, o.get('total_cost_usd') or 0
