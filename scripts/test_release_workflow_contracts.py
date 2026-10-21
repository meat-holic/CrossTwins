"""Offline producer/consumer contracts; never calls GitHub or builds firmware."""
import configparser
import json
import re
import shutil
import subprocess
import unittest
from pathlib import Path

from release_preflight import PROFILES
from release_version import DEVICES, asset_name, resolve_release

ROOT = Path(__file__).resolve().parents[1]
WORKFLOWS = ROOT / ".github/workflows"


def read(name):
    return (WORKFLOWS / name).read_text(encoding="utf-8-sig")


class ReleaseWorkflowContracts(unittest.TestCase):
    def test_rc_matrix_uses_exact_supported_environments(self):
        expected = {value[0] for value in PROFILES.values()}
        actual = re.findall(r"^\s+- pio_env: (\S+)$", read("release_candidate.yml"), re.M)
        self.assertEqual(len(actual), 5)
        self.assertEqual(set(actual), expected)
        ini = configparser.ConfigParser(interpolation=None)
        ini.read(ROOT / "platformio.ini", encoding="utf-8-sig")
        self.assertTrue(all(ini.has_section("env:" + env) for env in actual))

    def test_pr_artifact_consumer_matches_ci_producer(self):
        producer = re.findall(r"^\s+asset: (firmware[^\s]*\.bin)$", read("ci.yml"), re.M)
        consumer = re.findall(r"\{ name: '(firmware[^']*\.bin)'", read("pr-firmware-links.yml"))
        self.assertEqual(len(consumer), 5)
        self.assertEqual(set(producer), set(consumer))

    def test_preflight_publish_profiles_and_checksum_contract(self):
        self.assertEqual(set(re.findall(r"^\s+- env: (\S+)$", read("release.yml"), re.M)), {p[0] for p in PROFILES.values()})
        self.assertEqual(set(re.findall(r"^\s+- release_env: (\S+)$", read("release-publish.yml"), re.M)), {p[0] for p in PROFILES.values()})
        release = resolve_release("v1.6.5-ct.7", "1.6.5", False)
        names = {asset_name(release, d) for d in DEVICES}
        names.update(f"CrossPoint-v1.6.5-ct.7-{d}-full.bin" for d in DEVICES)
        names.update(("firmware.bin", "bootloader.bin", "partitions.bin"))
        self.assertEqual(len(names), 13)
        self.assertEqual(len({name.lower() for name in names}), 13)
        self.assertIn("sha256sum * > SHA256SUMS", read("release-publish.yml"))

    def test_actual_pr_script_with_five_missing_and_extra_mock_artifacts(self):
        node = shutil.which("node")
        self.assertIsNotNone(node, "Node is required for the existing PR script contract")
        script = read("pr-firmware-links.yml").split("          script: |\n", 1)[1]
        script = "\n".join(line[12:] if line.startswith(" " * 12) else line for line in script.splitlines())
        runner = r"""
const fs = require('node:fs');
const {script,names} = JSON.parse(fs.readFileSync(0,'utf8'));
(async () => {
 const bodies=[];
 const sha='a'.repeat(40), pr={number:2,state:'open',head:{sha}};
 const github={rest:{pulls:{list:async()=>({data:[pr]}),get:async()=>({data:pr})},
 actions:{listWorkflowRunArtifacts:async()=>({data:{artifacts:names.map((name,id)=>({name,id,expired:false}))}})},
 issues:{listComments:()=>{},createComment:async x=>bodies.push(x.body),updateComment:async x=>bodies.push(x.body)}},paginate:async()=>[]};
 const context={repo:{owner:'fixture',repo:'fixture'},payload:{workflow_run:{id:1,head_repository:{owner:{login:'fixture'}},head_sha:sha,head_branch:'branch',conclusion:'success',html_url:'https://example.invalid/run'}}};
 await new (Object.getPrototypeOf(async function(){}).constructor)('github','context','core',script)(github,context,{notice:()=>{}});
 process.stdout.write(JSON.stringify(bodies));
})().catch(e=>{console.error(e);process.exitCode=1;});
"""
        names = ["firmware.bin", "firmware-sticky.bin", "firmware-x4pro.bin", "firmware-x4c.bin", "firmware-papermono.bin"]
        for supplied, complete in [(names, True), (names[:-1], False), (names + ["firmware-picco.bin"], True)]:
            result = subprocess.run([node, "-e", runner], input=json.dumps({"script": script, "names": supplied}), text=True, capture_output=True, check=True)
            bodies = json.loads(result.stdout)
            self.assertEqual(len(bodies), 1)
            self.assertEqual("Firmware is not available" not in bodies[0], complete)
            if complete:
                self.assertEqual(bodies[0].count("/artifacts/"), 5)
                self.assertNotIn("firmware-picco.bin", bodies[0])
            else:
                self.assertIn("firmware-papermono.bin", bodies[0])


if __name__ == "__main__":
    unittest.main()
