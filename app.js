(() => {
  const root = document.documentElement;
  let owner = root.dataset.owner;
  const repository = root.dataset.repository || "HardwareScope";

  if (!owner || owner.startsWith("__")) {
    const match = location.hostname.match(/^([^.]+)\.github\.io$/i);
    if (match) owner = match[1];
  }

  if (!owner || owner.startsWith("__")) return;

  const repositoryUrl = `https://github.com/${owner}/${repository}`;
  const latestAssetUrl = (name) => `${repositoryUrl}/releases/latest/download/${encodeURIComponent(name)}`;

  document.querySelectorAll("[data-repo-link]").forEach((link) => {
    link.href = repositoryUrl;
  });
  document.querySelectorAll("[data-issues-link]").forEach((link) => {
    link.href = `${repositoryUrl}/issues`;
  });
  document.querySelectorAll("[data-release-asset]").forEach((link) => {
    link.href = latestAssetUrl(link.dataset.releaseAsset);
  });
})();
