// SPDX-License-Identifier: GPL-3.0-or-later
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::sync::OnceLock;

#[derive(Clone, Debug, Deserialize)]
pub struct BaseFile {
    pub url: String,
    pub filename: String,
    pub size: u64,
    pub sha256: String,
}

#[derive(Clone, Debug, Deserialize)]
pub struct ScFile {
    pub url: String,
    pub filename: String,
    pub size: u64,
    pub sha256: String,
    pub package_digest: String,
}

#[derive(Clone, Debug, Deserialize)]
pub struct Build {
    pub build: String,
    pub base: BaseFile,
    pub sc: HashMap<String, ScFile>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct FirmwareRange {
    pub from: String,
    pub to: String,
    pub build: String,
}

#[derive(Clone, Debug, Deserialize)]
pub struct Manifest {
    pub builds: Vec<Build>,
    pub firmware: Vec<FirmwareRange>,
    /// Versions inside the ranges that the Y2JB autoloader does not support.
    #[serde(default)]
    pub unsupported_firmware: Vec<String>,
}

/// The pinned data the app owns. It is compiled in, so a fetch can never be verified against
/// a manifest fetched at runtime.
const MANIFEST_JSON: &str = include_str!("../../../pkgs/youtube.json");

fn manifest() -> &'static Manifest {
    static MANIFEST: OnceLock<Manifest> = OnceLock::new();
    MANIFEST
        .get_or_init(|| serde_json::from_str(MANIFEST_JSON).expect("pkgs/youtube.json is invalid"))
}

pub fn build(name: &str) -> Option<&'static Build> {
    manifest().builds.iter().find(|build| build.build == name)
}

pub fn all() -> &'static Manifest {
    manifest()
}

#[derive(Serialize)]
pub struct RegionSummary {
    pub region: String,
    pub filename: String,
    pub size: u64,
    pub package_digest: String,
}

#[derive(Serialize)]
pub struct BuildSummary {
    pub build: String,
    pub base_filename: String,
    pub regions: Vec<RegionSummary>,
}

#[derive(Serialize)]
pub struct ManifestView {
    pub builds: Vec<BuildSummary>,
    pub firmware: Vec<FirmwareRange>,
    pub unsupported_firmware: Vec<String>,
}

/// The app's pinned data, so the UI can offer the builds and regions and map an installed
/// digest without duplicating the manifest.
#[tauri::command]
pub fn manifest_view() -> ManifestView {
    let data = all();
    ManifestView {
        builds: data
            .builds
            .iter()
            .map(|entry| {
                let mut regions: Vec<RegionSummary> = entry
                    .sc
                    .iter()
                    .map(|(region, sc)| RegionSummary {
                        region: region.clone(),
                        filename: sc.filename.clone(),
                        size: sc.size,
                        package_digest: sc.package_digest.clone(),
                    })
                    .collect();
                regions.sort_by(|left, right| left.region.cmp(&right.region));
                BuildSummary {
                    build: entry.build.clone(),
                    base_filename: entry.base.filename.clone(),
                    regions,
                }
            })
            .collect(),
        firmware: data.firmware.clone(),
        unsupported_firmware: data.unsupported_firmware.clone(),
    }
}

/// The build that matches the console's firmware string, or None when unsupported.
#[tauri::command]
pub fn expected_build(firmware: String) -> Option<String> {
    build_for_firmware(&firmware).map(|build| build.build.clone())
}

/// "13.00" becomes 1300, "4.03" becomes 403, so firmware ranges compare as integers.
pub fn firmware_code(firmware: &str) -> Option<u32> {
    let (major, minor) = firmware.split_once('.')?;
    Some(major.parse::<u32>().ok()? * 100 + minor.parse::<u32>().ok()?)
}

/// The build that matches a firmware string from the API, or None when unsupported.
pub fn build_for_firmware(firmware: &str) -> Option<&'static Build> {
    if manifest()
        .unsupported_firmware
        .iter()
        .any(|version| version == firmware)
    {
        return None;
    }
    let code = firmware_code(firmware)?;
    let range = manifest().firmware.iter().find(|range| {
        match (firmware_code(&range.from), firmware_code(&range.to)) {
            (Some(from), Some(to)) => code >= from && code <= to,
            _ => false,
        }
    })?;
    build(&range.build)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn is_lower_hex(value: &str) -> bool {
        value.len() == 64
            && value
                .bytes()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
    }

    fn is_upper_hex(value: &str) -> bool {
        value.len() == 64
            && value
                .bytes()
                .all(|b| b.is_ascii_digit() || (b'A'..=b'F').contains(&b))
    }

    #[test]
    fn firmware_codes_match_the_old_table() {
        assert_eq!(firmware_code("4.03"), Some(403));
        assert_eq!(firmware_code("13.00"), Some(1300));
        assert_eq!(firmware_code("13.60"), Some(1360));
        assert_eq!(firmware_code("nonsense"), None);
    }

    #[test]
    fn firmware_ranges_cover_the_supported_range() {
        assert_eq!(
            build_for_firmware("4.03").map(|b| b.build.as_str()),
            Some("v01.000.003")
        );
        assert_eq!(
            build_for_firmware("11.39").map(|b| b.build.as_str()),
            Some("v01.000.003")
        );
        assert!(build_for_firmware("11.40").is_none());
        assert_eq!(
            build_for_firmware("11.41").map(|b| b.build.as_str()),
            Some("v01.000.003")
        );
        assert_eq!(
            build_for_firmware("12.59").map(|b| b.build.as_str()),
            Some("v01.000.003")
        );
        assert_eq!(
            build_for_firmware("12.60").map(|b| b.build.as_str()),
            Some("v01.000.030")
        );
        assert_eq!(
            build_for_firmware("13.00").map(|b| b.build.as_str()),
            Some("v01.000.030")
        );
        assert_eq!(
            build_for_firmware("13.39").map(|b| b.build.as_str()),
            Some("v01.000.030")
        );
        assert_eq!(
            build_for_firmware("13.40").map(|b| b.build.as_str()),
            Some("v01.009.253")
        );
        assert_eq!(
            build_for_firmware("13.60").map(|b| b.build.as_str()),
            Some("v01.009.253")
        );
        assert!(build_for_firmware("4.02").is_none());
        assert!(build_for_firmware("3.00").is_none());
        assert!(build_for_firmware("13.61").is_none());
    }

    #[test]
    fn every_build_has_all_regions_and_well_formed_pins() {
        for build in &manifest().builds {
            assert!(
                is_lower_hex(&build.base.sha256),
                "{} base hash",
                build.build
            );
            assert!(build.base.size > 0);
            for region in ["usa", "europe", "japan"] {
                let sc = build
                    .sc
                    .get(region)
                    .unwrap_or_else(|| panic!("{} has no {region} SC", build.build));
                assert!(is_lower_hex(&sc.sha256), "{} {region} hash", build.build);
                assert!(
                    is_upper_hex(&sc.package_digest),
                    "{} {region} digest",
                    build.build
                );
                assert!(sc.size > 0);
            }
        }
    }
}
