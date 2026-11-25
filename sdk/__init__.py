"""Zero-dependency Python SDK for the ZKAEDI ZCC Sovereign Blackwell AI Studio API."""

from .zcc_client import ZccApiError, ZccClient

__all__ = ["ZccClient", "ZccApiError"]
