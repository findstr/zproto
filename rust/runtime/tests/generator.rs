use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::Once;

static BUILD_GENERATOR: Once = Once::new();

fn manifest_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
}

fn emitter_dir() -> PathBuf {
    manifest_dir().join("..").join("emitter")
}

fn build_generator() {
    BUILD_GENERATOR.call_once(|| {
        let status = Command::new("make")
            .arg("zproto-gen-rust")
            .current_dir(emitter_dir())
            .status()
            .expect("run make zproto-gen-rust");
        assert!(status.success(), "make zproto-gen-rust failed");
    });
}

fn temp_dir(name: &str) -> PathBuf {
    let mut path = std::env::temp_dir();
    path.push(format!("zproto-rust-{}-{}", name, std::process::id()));
    if path.exists() {
        fs::remove_dir_all(&path).unwrap();
    }
    fs::create_dir_all(&path).unwrap();
    path
}

fn write_schema(dir: &Path, name: &str, schema: &str) -> PathBuf {
    let path = dir.join(name);
    fs::write(&path, schema).unwrap();
    path
}

fn run_generator(schema: &Path) -> std::process::Output {
    Command::new(emitter_dir().join("zproto-gen-rust"))
        .arg(schema)
        .arg("--out")
        .arg(schema.parent().unwrap())
        .output()
        .expect("run zproto rust generator")
}

fn run_command(mut cmd: Command, what: &str) -> std::process::Output {
    let output = cmd.output().unwrap_or_else(|err| panic!("run {what}: {err}"));
    assert!(
        output.status.success(),
        "{what} failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    output
}

#[test]
fn generator_rejects_nested_structs() {
    build_generator();
    let dir = temp_dir("nested");
    let schema = write_schema(
        &dir,
        "bad.nested.zproto",
        r#"
packet 1 {
    inner {
        .id:integer 1
    }
    .id:integer 1
}
"#,
    );
    let output = run_generator(&schema);
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(stderr.contains("nested struct"), "stderr: {stderr}");
}

#[test]
fn generator_rejects_float_map_key() {
    build_generator();
    let dir = temp_dir("float-map");
    let schema = write_schema(
        &dir,
        "bad.float.zproto",
        r#"
entry {
    .key:float 1
    .value:integer 2
}
packet 1 {
    .entries:entry[key] 1
}
"#,
    );
    let output = run_generator(&schema);
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("unsupported map key") || stderr.contains("can't be mapkey"),
        "stderr: {stderr}"
    );
}

#[test]
fn generator_rejects_blob_map_key() {
    build_generator();
    let dir = temp_dir("blob-map");
    let schema = write_schema(
        &dir,
        "bad.blob.zproto",
        r#"
entry {
    .key:blob 1
    .value:integer 2
}
packet 1 {
    .entries:entry[key] 1
}
"#,
    );
    let output = run_generator(&schema);
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("unsupported map key") || stderr.contains("can't be mapkey"),
        "stderr: {stderr}"
    );
}

#[test]
fn generator_rejects_struct_map_key() {
    build_generator();
    let dir = temp_dir("struct-map");
    let schema = write_schema(
        &dir,
        "bad.struct.zproto",
        r#"
key_type {
    .id:integer 1
}
entry {
    .key:key_type 1
    .value:integer 2
}
packet 1 {
    .entries:entry[key] 1
}
"#,
    );
    let output = run_generator(&schema);
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("unsupported map key") || stderr.contains("can't be mapkey"),
        "stderr: {stderr}"
    );
}

#[test]
fn generated_code_round_trips() {
    build_generator();
    let dir = temp_dir("roundtrip");
    let schema = dir.join("hello.world.zproto");
    fs::copy(
        manifest_dir().join("tests/fixtures/hello.world.zproto"),
        &schema,
    )
    .unwrap();
    let output = Command::new(emitter_dir().join("zproto-gen-rust"))
        .arg(&schema)
        .arg("--out")
        .arg(&dir)
        .output()
        .expect("run generator");
    assert!(
        output.status.success(),
        "generator failed\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    let generated = fs::read_to_string(dir.join("hello_world.rs")).unwrap();
    assert!(
        generated.contains("use ::zproto::{self, Message};"),
        "generated runtime import should use absolute external crate path:\n{generated}"
    );
    assert!(
        !generated.contains("crate::zproto"),
        "generated code should not require a crate-local zproto bridge:\n{generated}"
    );

    let crate_dir = dir.join("use_generated");
    fs::create_dir_all(crate_dir.join("src")).unwrap();
    fs::copy(dir.join("hello_world.rs"), crate_dir.join("src/hello_world.rs")).unwrap();
    fs::write(
        crate_dir.join("Cargo.toml"),
        format!(
            r#"[package]
name = "use_generated"
version = "0.1.0"
edition = "2021"

[dependencies]
zproto = {{ path = "{}" }}
"#,
            manifest_dir().display()
        ),
    )
    .unwrap();
    fs::write(
        crate_dir.join("src/lib.rs"),
        r#"pub mod hello_world;

#[cfg(test)]
mod tests {
    use crate::hello_world::hello::world::{Item, Packet, PlayerLevel};
    use zproto::{self, Message};
    use std::collections::HashMap;

    fn sample() -> Packet {
        let mut friends = HashMap::new();
        friends.insert(7, PlayerLevel { userid: 7, level: 9 });
        Packet {
            userid: 42,
            name: "alice".to_string(),
            blob: vec![1, 2, 3, 0, 0],
            items: vec![Item { id: 1, count: 2 }, Item { id: 3, count: 4 }],
            friends,
            r#from: 99,
            r#type: "login".to_string(),
            flags: vec![true, false, true],
            score: -123,
            ratio: 1.5,
        }
    }

    #[test]
    fn generated_message_round_trips() {
        assert_eq!(Packet::TAG, 0xfe);
        assert_eq!(Packet::NAME, "packet");

        let packet = sample();
        let wire = packet.encode().unwrap();
        let decoded = Packet::decode(&wire).unwrap();
        assert_eq!(decoded, packet);

        let mut reusable = Vec::with_capacity(1024);
        reusable.extend_from_slice(&[0xaa, 0xbb]);
        let written = packet.encode_to(&mut reusable).unwrap();
        assert_eq!(written, reusable.len() - 2);

        let mut parsed = Packet::default();
        parsed.name = "old".to_string();
        parsed.items.push(Item { id: 99, count: 99 });
        let consumed = parsed.decode_from(&reusable[2..]).unwrap();
        assert_eq!(consumed, written);
        assert_eq!(parsed, packet);

        let mut packed = vec![0x55];
        let packed_written = zproto::pack_to(&wire, &mut packed);
        assert_eq!(packed_written, packed.len() - 1);

        let mut unpacked = vec![0x44];
        let unpacked_written = zproto::unpack_to(&packed[1..], &mut unpacked).unwrap();
        assert!(unpacked_written >= wire.len());
        assert_eq!(&unpacked[1..1 + wire.len()], wire.as_slice());
        assert_eq!(Packet::decode(&unpacked[1..]).unwrap(), packet);
    }
}
"#,
    )
    .unwrap();

    let mut cmd = Command::new("cargo");
    cmd.arg("test").current_dir(&crate_dir);
    run_command(cmd, "cargo test generated crate");
}
