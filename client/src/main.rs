use std::env;
use std::fs::File;
use std::io::{Error, ErrorKind, Read, Result, Seek, SeekFrom, Write};
use std::net::TcpStream;
use std::string::String;

static BIT_MODE: u32 = 1;

fn main() {
    send_image().unwrap()
}

fn send_image() -> Result<()> {
    let args: Vec<String> = env::args().collect();
    let mut file = File::open(&args[2])?;
    file.seek(SeekFrom::Start(2 + 4 + 2 + 2))?;
    let mut buffer = [0u8; 4];
    file.read_exact(&mut buffer)?;
    let offset = u32::from_le_bytes(buffer);
    file.seek(SeekFrom::Start(offset as u64))?;

    let width = 1200;
    let height = 825;
    let raw = if BIT_MODE == 3 {
        flip(&mut file, width, height)?
    } else {
        let dithered = dither_and_flip(&mut file, width, height)?;
        let mut packed: Vec<u8> = vec![0; width * height / 8];
        for y in 0..height {
            for x in (0..width).step_by(8) {
                let base = y * width + x;
                packed[base / 8] =
                    (dithered[base + 0] << 7) |
                    (dithered[base + 1] << 6) |
                    (dithered[base + 2] << 5) |
                    (dithered[base + 3] << 4) |
                    (dithered[base + 4] << 3) |
                    (dithered[base + 5] << 2) |
                    (dithered[base + 6] << 1) |
                    (dithered[base + 7] << 0);
            }
        }
        packed
    };

    let mut stream = TcpStream::connect(&args[1])?;
    return draw_pixels(&mut stream, &raw);
}

fn draw_pixels(stream: &mut TcpStream, buffer: &Vec<u8>) -> Result<()> {
    stream.write(if BIT_MODE == 3 { b"3" } else { b"1" })?;
    stream.write(&(buffer.len() as u32).to_be_bytes())?;
    stream.write(&buffer)?;
    return check_response(stream, "pixels");
}

fn check_response(stream: &mut TcpStream, item: &str) -> Result<()> {
    let mut r: Vec<u8> = vec![0; 1];
    loop {
        let read = stream.read(&mut r)?;
        if read > 0 {
            break;
        }
    }
    if r[0] != 'y' as u8 {
        println!("{}", r[0]);
        return Err(Error::new(ErrorKind::Other, String::from("Server did not accept ") + item));
    }
    return Ok(());
}

fn flip(source: &mut File, width: usize, height: usize) -> Result<Vec<u8>> {
    let mut data: Vec<u8> = vec![0; width * height];
    let mut buffer = [0u8; 1];
    for y in (0..height).rev() {
        for x in 0..width {
            source.read_exact(&mut buffer)?;
            data[y * width + x] = buffer[0];
        }
    }
    return Ok(data);
}

fn dither_and_flip(source: &mut File, width: usize, height: usize) -> Result<Vec<u8>> {
    let mut data: Vec<u8> = vec![0; width * height];
    let mut errors: Vec<f32> = vec![0.; width * height];
    let mut buffer = [0u8; 1];
    for y in (0..height).rev() {
        for x in 0..width {
            source.read_exact(&mut buffer)?;
            let old = (errors[y * width + x] + (buffer[0] as f32)) as u8;
            let new = if old < 128 { 0 } else { 1 };
            data[y * width + x] = 1 - new;
            let error = ((old as f32) - (new as f32) * 255.) / 16.;

            if x < width - 1 {
                errors[y * width + x + 1] += error * 7.;
            }
            if y > 0 {
                if 0 < x {
                    errors[(y - 1) * width + x - 1] += error * 3.;
                }
                errors[(y - 1) * width + x] += error * 5.;
                if x < width - 1 {
                    errors[(y - 1) * width + x + 1] += error * 1.;
                }
            }
        }
    }
    return Ok(data);
}
