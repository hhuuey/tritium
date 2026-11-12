from pathlib import Path
from urllib.parse import unquote, urlsplit

from asset_protocol import ArtifactRef


def resolve_artifact_path(reference: ArtifactRef, *, strict: bool = True) -> Path:
    uri = reference.uri
    parsed = urlsplit(uri)
    value = unquote(parsed.path) if parsed.scheme == "file" else uri
    return Path(value).expanduser().resolve(strict=strict)
