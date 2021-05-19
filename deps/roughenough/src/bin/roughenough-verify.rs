// Copyright 2017-2019 int08h LLC

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
    Roughtime transcript verifier 
    (copied mostly from roughenough-client.rs)

    Author: Deepak
*/

extern crate itertools;
use itertools::Itertools;

use byteorder::{LittleEndian, ReadBytesExt};

use chrono::{TimeZone, Local};

use std::collections::HashMap;
use std::fs::File;
use std::io::Read;

use clap::{App, Arg};
use roughenough::merkle::root_from_paths;
use roughenough::sign::Verifier;
use roughenough::{
    roughenough_version, RtMessage, Tag, CERTIFICATE_CONTEXT, SIGNED_RESPONSE_CONTEXT,
};

struct ResponseHandler {
    pub_key: Option<Vec<u8>>,
    msg: HashMap<Tag, Vec<u8>>,
    srep: HashMap<Tag, Vec<u8>>,
    cert: HashMap<Tag, Vec<u8>>,
    dele: HashMap<Tag, Vec<u8>>,
    nonce: [u8; 64],
}

struct ParsedResponse<'a> {
    verified: bool,
    midpoint: u64,
    radius: u32,
    signature: &'a [u8]
}

impl ResponseHandler {
    pub fn new(pub_key: Option<Vec<u8>>, response: RtMessage, nonce: [u8; 64]) -> ResponseHandler {
        let msg = response.into_hash_map();
        let srep = RtMessage::from_bytes(&msg[&Tag::SREP])
            .unwrap()
            .into_hash_map();
        let cert = RtMessage::from_bytes(&msg[&Tag::CERT])
            .unwrap()
            .into_hash_map();
        let dele = RtMessage::from_bytes(&cert[&Tag::DELE])
            .unwrap()
            .into_hash_map();

        ResponseHandler {
            pub_key,
            msg,
            srep,
            cert,
            dele,
            nonce,
        }
    }

    pub fn extract_time(&self) -> ParsedResponse {
        let midpoint = self.srep[&Tag::MIDP]
            .as_slice()
            .read_u64::<LittleEndian>()
            .unwrap();
        let radius = self.srep[&Tag::RADI]
            .as_slice()
            .read_u32::<LittleEndian>()
            .unwrap();
        let signature = &self.msg[&Tag::SIG];

        let verified = if self.pub_key.is_some() {
            self.validate_dele();
            self.validate_srep();
            self.validate_merkle();
            self.validate_midpoint(midpoint);
            true
        } else {
            false
        };

        ParsedResponse {
            verified,
            midpoint,
            radius,
            signature
        }
    }

    fn validate_dele(&self) {
        let mut full_cert = Vec::from(CERTIFICATE_CONTEXT.as_bytes());
        full_cert.extend(&self.cert[&Tag::DELE]);

        assert!(
            self.validate_sig(
                self.pub_key.as_ref().unwrap(),
                &self.cert[&Tag::SIG],
                &full_cert
            ),
            "Invalid signature on DELE tag, response may not be authentic"
        );
    }

    fn validate_srep(&self) {
        let mut full_srep = Vec::from(SIGNED_RESPONSE_CONTEXT.as_bytes());
        full_srep.extend(&self.msg[&Tag::SREP]);

        assert!(
            self.validate_sig(&self.dele[&Tag::PUBK], &self.msg[&Tag::SIG], &full_srep),
            "Invalid signature on SREP tag, response may not be authentic"
        );
    }

    fn validate_merkle(&self) {
        let srep = RtMessage::from_bytes(&self.msg[&Tag::SREP])
            .unwrap()
            .into_hash_map();
        let index = self.msg[&Tag::INDX]
            .as_slice()
            .read_u32::<LittleEndian>()
            .unwrap();
        let paths = &self.msg[&Tag::PATH];

        let hash = root_from_paths(index as usize, &self.nonce, paths);

        assert_eq!(
            hash, srep[&Tag::ROOT],
            "Nonce is not present in the response's merkle tree"
        );
    }

    fn validate_midpoint(&self, midpoint: u64) {
        let mint = self.dele[&Tag::MINT]
            .as_slice()
            .read_u64::<LittleEndian>()
            .unwrap();
        let maxt = self.dele[&Tag::MAXT]
            .as_slice()
            .read_u64::<LittleEndian>()
            .unwrap();

        assert!(
            midpoint >= mint,
            "Response midpoint {} lies *before* delegation span ({}, {})",
            midpoint, mint, maxt
        );
        assert!(
            midpoint <= maxt,
            "Response midpoint {} lies *after* delegation span ({}, {})",
            midpoint, mint, maxt
        );
    }

    fn validate_sig(&self, public_key: &[u8], sig: &[u8], data: &[u8]) -> bool {
        let mut verifier = Verifier::new(public_key);
        verifier.update(data);
        verifier.verify(sig)
    }
}

fn main() {
    let matches = App::new("roughenough transcript verifier")
    .version(roughenough_version().as_ref())
    .arg(Arg::with_name("nonce")
      .short("c")
      .long("nonce")
      .takes_value(true)
      .help("Specify input nonce. If unset, a random nonce will be used.")
    )
    .arg(Arg::with_name("verbose")
      .short("v")
      .long("verbose")
      .help("Output additional details about the server's response."))
    .arg(Arg::with_name("public-key")
      .short("p")
      .long("public-key")
      .takes_value(true)
      .help("The server public key used to validate responses. If unset, no validation will be performed."))
    .arg(Arg::with_name("time-format")
      .short("f")
      .long("time-format")
      .takes_value(true)
      .help("The strftime format string used to print the time recieved from the server.")
      .default_value("%b %d %Y %H:%M:%S %Z")
    )
    .arg(Arg::with_name("transcript")
      .short("t")
      .long("transcript")
      .takes_value(true)
      .help("Writes all responses to the specified file.")
    )
    .get_matches();

    let inp_nonce = matches
        .value_of("nonce")
        .map(|nc| hex::decode(nc).expect("Error parsing nonce!"));
    let verbose = matches.is_present("verbose");
    let time_format = matches.value_of("time-format").unwrap();
    let pub_key = matches
        .value_of("public-key")
        .map(|pkey| hex::decode(pkey).expect("Error parsing public key!"));
    let resp_out = matches.value_of("transcript");

    // for _ in 0..num_requests {
    let nonce = if inp_nonce.is_some() {
        let x = inp_nonce.unwrap();
        if x.len() != 64 {
            panic!("Exp 64. Input nonce length: {}", x.len());
        }
        x
    } else {
        panic!("Specify nonce used!");
    };

    let mut resp_file = resp_out.map(|o| File::open(o).expect("Failed to open file!")).unwrap();

    let mut buf = [0; 744];
    let n = resp_file.read(&mut buf).expect("Failed to read file!");
    let resp = RtMessage::from_bytes(&buf[0..n]).unwrap();

    let mut arr_nonce: [u8; 64] = [0; 64];
    arr_nonce.copy_from_slice(&nonce[0..64]);

    let d = ResponseHandler::new(pub_key.clone(), resp.clone(), arr_nonce);
    let ParsedResponse {
        verified,
        midpoint,
        radius,
        signature,
    } = d.extract_time();

    if verbose {
        println!("[Roughtime] Nonce: {:02x}", nonce.iter().format(""));
        println!("[Roughtime] Sign: {:02x}", signature.iter().format(""));    
    }
    
    let map = resp.into_hash_map();
    let index = map[&Tag::INDX]
        .as_slice()
        .read_u32::<LittleEndian>()
        .unwrap();

    let seconds = midpoint / 10_u64.pow(6);
    let nsecs = (midpoint - (seconds * 10_u64.pow(6))) * 10_u64.pow(3);
    let verify_str = if verified { "Yes" } else { "No" };

    println!("{}", verified);

    let out = {
        let ts = Local.timestamp(seconds as i64, nsecs as u32);
        ts.format(time_format).to_string()
    };

    if verbose {
        eprintln!(
            "Received time from server: midpoint={:?}, radius={:?}, verified={} (merkle_index={})",
            out, radius, verify_str, index
        );
    }
}

