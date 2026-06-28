#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    Eof,
    Malformed,
    Utf8,
    Unsupported(&'static str),
}

pub trait Message: Sized + Default {
    const TAG: i32;
    const NAME: &'static str;

    fn reset(&mut self);

    #[inline]
    fn encode(&self) -> Result<Vec<u8>, Error> {
        let mut out = Vec::new();
        self.encode_to(&mut out)?;
        Ok(out)
    }

    fn encode_to(&self, out: &mut Vec<u8>) -> Result<usize, Error>;

    #[inline]
    fn decode(data: &[u8]) -> Result<Self, Error> {
        let mut value = Self::default();
        value.decode_from(data)?;
        Ok(value)
    }

    fn decode_from(&mut self, data: &[u8]) -> Result<usize, Error>;
}

pub struct Encoder {
    start: usize,
    tag_count_off: usize,
    tag_off: usize,
    body_off: usize,
    last_tag: i32,
    present_count: usize,
}

impl Encoder {
    #[inline]
    pub fn new(out: &mut Vec<u8>, basetag: i32, fieldcount: usize) -> Self {
        let start = out.len();
        out.extend_from_slice(&0u32.to_le_bytes());
        let tag_count_off = out.len();
        out.extend_from_slice(&0u16.to_le_bytes());
        let tag_off = out.len();
        out.resize(out.len() + fieldcount * 2, 0);
        let body_off = out.len();
        Self {
            start,
            tag_count_off,
            tag_off,
            body_off,
            last_tag: basetag - 1,
            present_count: 0,
        }
    }

    #[inline]
    pub fn present(&mut self, out: &mut [u8], tag: i32) {
        let delta = (tag - self.last_tag - 1) as u16;
        out[self.tag_off..self.tag_off + 2].copy_from_slice(&delta.to_le_bytes());
        self.tag_off += 2;
        self.last_tag = tag;
        self.present_count += 1;
    }

    #[inline]
    pub fn w_u8(&mut self, out: &mut Vec<u8>, v: u8) {
        out.push(v);
    }

    #[inline]
    pub fn w_u16(&mut self, out: &mut Vec<u8>, v: u16) {
        out.extend_from_slice(&v.to_le_bytes());
    }

    #[inline]
    pub fn w_u32(&mut self, out: &mut Vec<u8>, v: u32) {
        out.extend_from_slice(&v.to_le_bytes());
    }

    #[inline]
    pub fn w_u64(&mut self, out: &mut Vec<u8>, v: u64) {
        out.extend_from_slice(&v.to_le_bytes());
    }

    #[inline]
    pub fn w_f32(&mut self, out: &mut Vec<u8>, v: f32) {
        self.w_u32(out, v.to_bits());
    }

    #[inline]
    pub fn w_string(&mut self, out: &mut Vec<u8>, v: &str) {
        self.w_bytes(out, v.as_bytes());
    }

    #[inline]
    pub fn w_bytes(&mut self, out: &mut Vec<u8>, v: &[u8]) {
        self.w_u32(out, v.len() as u32);
        out.extend_from_slice(v);
    }

    #[inline]
    pub fn w_array(&mut self, out: &mut Vec<u8>, count: u32) {
        self.w_u32(out, count);
    }

    #[inline]
    pub fn finish(self, out: &mut Vec<u8>) -> Result<usize, Error> {
        if self.present_count > u16::MAX as usize {
            return Err(Error::Malformed);
        }

        let present_count = self.present_count as u16;
        out[self.tag_count_off..self.tag_count_off + 2]
            .copy_from_slice(&present_count.to_le_bytes());

        let body_size = out.len() - self.body_off;
        let used_tag_end = self.tag_off;
        if used_tag_end != self.body_off {
            out.copy_within(self.body_off..self.body_off + body_size, used_tag_end);
            out.truncate(used_tag_end + body_size);
        }

        let data_size = (out.len() - self.start - 4) as u32;
        out[self.start..self.start + 4].copy_from_slice(&data_size.to_le_bytes());
        Ok(4 + data_size as usize)
    }
}

#[derive(Debug)]
pub struct Decoder<'a> {
    data: &'a [u8],
    cur: usize,
    end: usize,
    tag_ptr: usize,
    tags_left: usize,
    last_tag: i32,
}

impl<'a> Decoder<'a> {
    #[inline]
    pub fn new(data: &'a [u8], basetag: i32) -> Result<Self, Error> {
        if data.len() < 4 {
            return Err(Error::Eof);
        }
        let data_size = read_u32_at(data, 0)? as usize;
        let end = 4usize.checked_add(data_size).ok_or(Error::Malformed)?;
        if data.len() < end {
            return Err(Error::Eof);
        }
        if end < 6 {
            return Err(Error::Malformed);
        }
        let tag_count = read_u16_at(data, 4)? as usize;
        let tag_bytes = tag_count.checked_mul(2).ok_or(Error::Malformed)?;
        let body_off = 6usize.checked_add(tag_bytes).ok_or(Error::Malformed)?;
        if body_off > end {
            return Err(Error::Malformed);
        }
        Ok(Self {
            data,
            cur: body_off,
            end,
            tag_ptr: 6,
            tags_left: tag_count,
            last_tag: basetag - 1,
        })
    }

    #[inline]
    pub fn next(&mut self) -> Result<Option<i32>, Error> {
        if self.tags_left == 0 {
            return Ok(None);
        }
        let delta = self.read_tag_delta()? as i32;
        self.last_tag += delta + 1;
        self.tags_left -= 1;
        Ok(Some(self.last_tag))
    }

    #[inline]
    pub fn r_u8(&mut self) -> Result<u8, Error> {
        let bytes = self.take(1)?;
        Ok(bytes[0])
    }

    #[inline]
    pub fn r_u16(&mut self) -> Result<u16, Error> {
        let bytes = self.take(2)?;
        Ok(u16::from_le_bytes([bytes[0], bytes[1]]))
    }

    #[inline]
    pub fn r_u32(&mut self) -> Result<u32, Error> {
        let bytes = self.take(4)?;
        Ok(u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]))
    }

    #[inline]
    pub fn r_u64(&mut self) -> Result<u64, Error> {
        let bytes = self.take(8)?;
        Ok(u64::from_le_bytes([
            bytes[0], bytes[1], bytes[2], bytes[3],
            bytes[4], bytes[5], bytes[6], bytes[7],
        ]))
    }

    #[inline]
    pub fn r_f32(&mut self) -> Result<f32, Error> {
        Ok(f32::from_bits(self.r_u32()?))
    }

    #[inline]
    pub fn r_string(&mut self) -> Result<String, Error> {
        String::from_utf8(self.r_bytes()?).map_err(|_| Error::Utf8)
    }

    #[inline]
    pub fn r_bytes(&mut self) -> Result<Vec<u8>, Error> {
        let len = self.r_u32()? as usize;
        Ok(self.take(len)?.to_vec())
    }

    #[inline]
    pub fn r_array(&mut self) -> Result<u32, Error> {
        self.r_u32()
    }

    #[inline]
    pub fn struct_bytes(&mut self) -> Result<&'a [u8], Error> {
        let start = self.cur;
        let data_size = read_u32_at(&self.data[start..self.end], 0)? as usize;
        let total = 4usize.checked_add(data_size).ok_or(Error::Malformed)?;
        let end = start.checked_add(total).ok_or(Error::Malformed)?;
        if end > self.end {
            return Err(Error::Eof);
        }
        self.cur = end;
        Ok(&self.data[start..end])
    }

    #[inline]
    pub fn size(&self) -> usize {
        self.end
    }

    #[inline]
    fn read_tag_delta(&mut self) -> Result<u16, Error> {
        let delta = read_u16_at(self.data, self.tag_ptr)?;
        self.tag_ptr += 2;
        Ok(delta)
    }

    #[inline]
    fn take(&mut self, n: usize) -> Result<&'a [u8], Error> {
        let end = self.cur.checked_add(n).ok_or(Error::Malformed)?;
        if end > self.end {
            return Err(Error::Eof);
        }
        let bytes = &self.data[self.cur..end];
        self.cur = end;
        Ok(bytes)
    }
}

#[inline]
fn read_u16_at(data: &[u8], off: usize) -> Result<u16, Error> {
    if data.len() < off + 2 {
        return Err(Error::Eof);
    }
    Ok(u16::from_le_bytes([data[off], data[off + 1]]))
}

#[inline]
fn read_u32_at(data: &[u8], off: usize) -> Result<u32, Error> {
    if data.len() < off + 4 {
        return Err(Error::Eof);
    }
    Ok(u32::from_le_bytes([
        data[off],
        data[off + 1],
        data[off + 2],
        data[off + 3],
    ]))
}

#[inline]
pub fn pack(src: &[u8]) -> Vec<u8> {
    let needn = ((src.len() + 2047) / 2048) * 2 + src.len() + 1;
    let mut out = Vec::with_capacity(needn);
    pack_to(src, &mut out);
    out
}

#[inline]
pub fn pack_to(src: &[u8], out: &mut Vec<u8>) -> usize {
    let start = out.len();
    let mut pos = 0;
    let mut packed_size = -1i32;
    let mut full_run_count_pos = 0usize;

    while pos < src.len() {
        if packed_size != 8 {
            packed_size = pack_segment(&src[pos..], out) as i32;
            pos = pos.saturating_add(8);
            if packed_size == 8 && pos < src.len() {
                full_run_count_pos = out.len();
                out.push(0);
            }
        } else {
            out[full_run_count_pos] = 0;
            loop {
                let next = pack_full_run_segment(&src[pos..], out);
                if next >= 6 {
                    out[full_run_count_pos] = out[full_run_count_pos].wrapping_add(1);
                    if out[full_run_count_pos] == 255 {
                        // pack_full_run_segment already consumed `next` bytes
                        // and wrote them to output; advance pos to avoid
                        // re-packing the same segment in the outer loop.
                        pos = pos.saturating_add(next);
                        packed_size = -1;
                        break;
                    }
                    if pos.saturating_add(8) >= src.len() {
                        pos = src.len();
                        packed_size = -1;
                        break;
                    }
                    pos = pos.saturating_add(8);
                } else {
                    packed_size = next as i32;
                    break;
                }
            }
        }
    }

    out.len() - start
}

#[inline]
pub fn unpack(src: &[u8]) -> Result<Vec<u8>, Error> {
    // Pre-allocate ~2x src: zero-compression ratio is rarely above 50%.
    let mut out = Vec::with_capacity(src.len().saturating_mul(2));
    unpack_to(src, &mut out)?;
    Ok(out)
}

#[inline]
pub fn unpack_to(src: &[u8], out: &mut Vec<u8>) -> Result<usize, Error> {
    let start_len = out.len();
    match unpack_to_inner(src, out) {
        Ok(written) => Ok(written),
        Err(err) => {
            out.truncate(start_len);
            Err(err)
        }
    }
}

#[inline]
fn pack_segment(src: &[u8], out: &mut Vec<u8>) -> usize {
    let count = src.len().min(8);
    let header_pos = out.len();
    out.push(0);
    let mut header = 0u8;
    let mut packed = 0usize;

    for (i, byte) in src.iter().copied().take(count).enumerate() {
        if byte != 0 {
            header |= 1 << i;
            out.push(byte);
            packed += 1;
        }
    }

    out[header_pos] = header;
    packed
}

#[inline]
fn pack_full_run_segment(src: &[u8], out: &mut Vec<u8>) -> usize {
    let count = src.len().min(8);
    let packed = src.iter().copied().take(count).filter(|b| *b != 0).count();
    if packed >= 6 {
        out.extend_from_slice(&src[..count]);
        count
    } else {
        packed
    }
}

#[inline]
fn unpack_to_inner(src: &[u8], out: &mut Vec<u8>) -> Result<usize, Error> {
    let start_len = out.len();
    let mut pos = 0usize;
    let mut unpacked_size = -1i32;
    let mut full_run_count = 0usize;

    while pos < src.len() {
        if unpacked_size != 8 {
            let (consumed, unpacked) = unpack_segment(&src[pos..], out)?;
            pos += consumed;
            unpacked_size = unpacked as i32;
            if unpacked_size == 8 && pos < src.len() {
                full_run_count = src[pos] as usize;
                pos += 1;
            }
        } else {
            // count == 0 means no additional full-run segments; just
            // fall through to normal segment processing.
            if full_run_count == 0 {
                unpacked_size = -1;
                continue;
            }
            let run = full_run_count.checked_mul(8).ok_or(Error::Malformed)?;
            // ffn-1, because the last ff pack size may be 6, 7, or 8
            let safe_run = full_run_count
                .checked_sub(1)
                .and_then(|v| v.checked_mul(8))
                .ok_or(Error::Malformed)?;
            let remaining = src.len() - pos;
            if safe_run > remaining {
                return Err(Error::Malformed);
            }
            let copy = run.min(remaining);
            out.extend_from_slice(&src[pos..pos + copy]);
            if copy < run {
                out.resize(out.len() + (run - copy), 0);
            }
            pos += copy;
            unpacked_size = -1;
        }
    }

    Ok(out.len() - start_len)
}

#[inline]
fn unpack_segment(src: &[u8], out: &mut Vec<u8>) -> Result<(usize, usize), Error> {
    if src.is_empty() {
        return Err(Error::Eof);
    }

    let header = src[0];
    if header == 0 {
        out.extend_from_slice(&[0u8; 8]);
        return Ok((1, 0));
    }

    let mut data_pos = 1usize;
    let mut mask = header;
    let mut unpacked = 0usize;
    for _ in 0..8 {
        if mask & 1 != 0 {
            if data_pos >= src.len() {
                return Err(Error::Malformed);
            }
            out.push(src[data_pos]);
            data_pos += 1;
            unpacked += 1;
        } else {
            out.push(0);
        }
        mask >>= 1;
    }

    Ok((data_pos, unpacked))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn runtime_functions_are_marked_inline() {
        let source = include_str!("zproto.rs");
        let runtime_source = source
            .split("\n#[cfg(test)]")
            .next()
            .expect("runtime source before tests");
        let lines: Vec<&str> = runtime_source.lines().collect();
        let mut missing = Vec::new();

        for (idx, line) in lines.iter().enumerate() {
            let trimmed = line.trim_start();
            let is_fn = (trimmed.starts_with("fn ")
                || trimmed.starts_with("pub fn ")
                || trimmed.starts_with("pub(crate) fn "))
                && !trimmed.ends_with(';');
            if !is_fn {
                continue;
            }

            let prev = lines[..idx]
                .iter()
                .rev()
                .map(|line| line.trim())
                .find(|line| !line.is_empty())
                .unwrap_or("");
            if prev != "#[inline]" {
                missing.push(format!("{}: {}", idx + 1, trimmed));
            }
        }

        assert!(
            missing.is_empty(),
            "runtime functions missing #[inline]:\n{}",
            missing.join("\n")
        );
    }

    #[test]
    fn pack_to_appends_and_returns_written_len() {
        let input = [0u8; 8];
        let mut out = vec![0xaa, 0xbb];
        let written = pack_to(&input, &mut out);
        assert_eq!(written, 1);
        assert_eq!(out, vec![0xaa, 0xbb, 0x00]);
    }

    #[test]
    fn unpack_to_appends_and_returns_written_len() {
        let packed = [0x00u8];
        let mut out = vec![0xaa];
        let written = unpack_to(&packed, &mut out).unwrap();
        assert_eq!(written, 8);
        assert_eq!(&out[..1], &[0xaa]);
        assert_eq!(&out[1..], &[0u8; 8]);
    }

    #[test]
    fn unpack_to_rolls_back_on_error() {
        let mut out = vec![1, 2, 3];
        let err = unpack_to(&[0xff], &mut out).unwrap_err();
        assert_eq!(err, Error::Malformed);
        assert_eq!(out, vec![1, 2, 3]);
    }

    #[test]
    fn pack_unpack_matches_reference_vectors() {
        let cases: &[(&[u8], &[u8], &[u8])] = &[
            (&[0, 0, 0, 0, 0, 0, 0, 0], &[0x00], &[0, 0, 0, 0, 0, 0, 0, 0]),
            (
                &[0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff],
                &[0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                  0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff],
                &[0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff],
            ),
            (
                &[1, 0, 2, 0, 3, 0, 4, 0, 0, 5],
                &[0x55, 1, 2, 3, 4, 0x02, 5],
                &[1, 0, 2, 0, 3, 0, 4, 0, 0, 5, 0, 0, 0, 0, 0, 0],
            ),
            (
                &[1, 2, 3, 4, 5, 6, 7, 8,
                  9, 10, 11, 12, 13, 14, 15, 16,
                  17, 18, 19, 20, 21, 22, 23, 24],
                &[0xff, 1, 2, 3, 4, 5, 6, 7, 8, 0x02,
                  9, 10, 11, 12, 13, 14, 15, 16,
                  17, 18, 19, 20, 21, 22, 23, 24],
                &[1, 2, 3, 4, 5, 6, 7, 8,
                  9, 10, 11, 12, 13, 14, 15, 16,
                  17, 18, 19, 20, 21, 22, 23, 24],
            ),
            // full-run tail: 14 bytes, last segment 6 bytes (< 8, packed>=6)
            // C does NOT pad; packed output is 0xff + 8 + count(1) + 6 = 16 bytes
            (
                &[1, 2, 3, 4, 5, 6, 7, 8,
                  9, 10, 11, 12, 13, 14],
                &[0xff, 1, 2, 3, 4, 5, 6, 7, 8, 0x01,
                  9, 10, 11, 12, 13, 14],
                &[1, 2, 3, 4, 5, 6, 7, 8,
                  9, 10, 11, 12, 13, 14, 0, 0],
            ),
            // full-run tail: 15 bytes, last segment 7 bytes (< 8, packed>=6)
            (
                &[1, 2, 3, 4, 5, 6, 7, 8,
                  9, 10, 11, 12, 13, 14, 15],
                &[0xff, 1, 2, 3, 4, 5, 6, 7, 8, 0x01,
                  9, 10, 11, 12, 13, 14, 15],
                &[1, 2, 3, 4, 5, 6, 7, 8,
                  9, 10, 11, 12, 13, 14, 15, 0],
            ),
            (
                &[1, 2, 3, 4, 5, 6, 7, 8,
                  9, 10, 11, 12, 13, 0, 0, 0],
                &[0xff, 1, 2, 3, 4, 5, 6, 7, 8, 0x00,
                  0x1f, 9, 10, 11, 12, 13],
                &[1, 2, 3, 4, 5, 6, 7, 8,
                  9, 10, 11, 12, 13, 0, 0, 0],
            ),
        ];

        for (input, expected_pack, expected_unpack) in cases {
            assert_eq!(pack(input), *expected_pack);
            assert_eq!(unpack(expected_pack).unwrap(), *expected_unpack);
        }
    }

    #[test]
    fn encoder_decoder_round_trips_scalars_and_bytes() {
        let mut out = Vec::new();
        let mut e = Encoder::new(&mut out, 1, 4);
        e.present(&mut out, 1);
        e.w_u32(&mut out, 0x11223344);
        e.present(&mut out, 2);
        e.w_f32(&mut out, 1.5);
        e.present(&mut out, 3);
        e.w_string(&mut out, "hi");
        e.present(&mut out, 4);
        e.w_bytes(&mut out, &[1, 2, 3]);
        let written = e.finish(&mut out).unwrap();
        assert_eq!(written, out.len());

        let mut d = Decoder::new(&out, 1).unwrap();
        assert_eq!(d.next().unwrap(), Some(1));
        assert_eq!(d.r_u32().unwrap(), 0x11223344);
        assert_eq!(d.next().unwrap(), Some(2));
        assert_eq!(d.r_f32().unwrap(), 1.5);
        assert_eq!(d.next().unwrap(), Some(3));
        assert_eq!(d.r_string().unwrap(), "hi");
        assert_eq!(d.next().unwrap(), Some(4));
        assert_eq!(d.r_bytes().unwrap(), vec![1, 2, 3]);
        assert_eq!(d.next().unwrap(), None);
    }

    #[test]
    fn decoder_rejects_short_message() {
        assert_eq!(Decoder::new(&[0, 1, 2], 1).unwrap_err(), Error::Eof);
    }
}
