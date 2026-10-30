// SPDX-License-Identifier: GPL-3.0-or-later
use std::time::Duration;

use serde::Deserialize;

pub const DEFAULT_DOWNLOAD0_REPO: &str = "itsPLK/ps5-y2jb-autoloader";

#[derive(Deserialize)]
struct GithubAsset {
    name: String,
    size: u64,
    digest: Option<String>,
    browser_download_url: String,
}

#[derive(Deserialize)]
struct GithubRelease {
    tag_name: String,
    #[serde(default)]
    published_at: String,
    prerelease: bool,
    assets: Vec<GithubAsset>,
}

#[derive(Clone, Debug)]
pub struct ReleaseAsset {
    pub tag: String,
    pub size: u64,
    pub digest: String,
    pub url: String,
    pub published_at: String,
    pub prerelease: bool,
}

pub async fn release_assets(repo: Option<&str>) -> Result<Vec<ReleaseAsset>, String> {
    let repo_slug = repo.unwrap_or(DEFAULT_DOWNLOAD0_REPO).trim();
    let url = format!("https://api.github.com/repos/{repo_slug}/releases?per_page=30");

    let client = reqwest::Client::builder()
        .connect_timeout(Duration::from_secs(20))
        .timeout(Duration::from_secs(120))
        .build()
        .map_err(|error| error.to_string())?;
    let releases: Vec<GithubRelease> = client
        .get(&url)
        .header(reqwest::header::USER_AGENT, crate::USER_AGENT)
        .send()
        .await
        .map_err(|error| format!("GitHub: {error}"))?
        .error_for_status()
        .map_err(|error| format!("GitHub: {error}"))?
        .json()
        .await
        .map_err(|error| format!("GitHub: {error}"))?;

    let mut assets = Vec::new();
    for release in releases {
        for asset in &release.assets {
            if asset.name != "download0.dat" || !asset.browser_download_url.starts_with("https://")
            {
                continue;
            }
            let Some(digest) = asset
                .digest
                .as_deref()
                .and_then(|value| value.strip_prefix("sha256:"))
            else {
                continue;
            };
            assets.push(ReleaseAsset {
                tag: release.tag_name.clone(),
                size: asset.size,
                digest: digest.to_string(),
                url: asset.browser_download_url.clone(),
                published_at: release.published_at.clone(),
                prerelease: release.prerelease,
            });
            break;
        }
    }
    Ok(assets)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    #[ignore = "needs GitHub"]
    async fn lists_verifiable_releases() {
        let releases = release_assets(None).await.expect("release list failed");
        assert!(releases.len() >= 3);
        assert!(releases
            .iter()
            .all(|release| release.digest.len() == 64 && release.size > 0));
        assert!(releases.iter().any(|release| !release.prerelease));
    }
}
