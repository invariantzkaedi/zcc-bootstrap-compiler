#!/bin/bash
python3 -c 'from PIL import Image; print("PIL_OK")' 2>/dev/null || echo "NO_PIL"
