#!/bin/bash
#
# Model installation helper for AIE-OS
# Copies an ONNX model to the system share and updates configuration.
#
# Usage: sudo ./install_model.sh /path/to/model.onnx
#
set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 /path/to/model.onnx"
    exit 1
fi

MODEL_SRC="$1"
DESTDIR="/usr/local/share/aie-os"
CONFIG="/etc/aie-os/aie.conf"

if [ ! -f "$MODEL_SRC" ]; then
    echo "Model file not found: $MODEL_SRC"
    exit 1
fi

mkdir -p "$DESTDIR"
cp "$MODEL_SRC" "$DESTDIR/" || {
    echo "Failed to copy model"
    exit 1
}

MODEL_NAME=$(basename "$MODEL_SRC")

echo "Copied model to $DESTDIR/$MODEL_NAME"

# update configuration file if present
if [ -f "$CONFIG" ]; then
    sed -i "s|^model_path=.*|model_path=$DESTDIR/$MODEL_NAME|" "$CONFIG"
    echo "Updated model_path in $CONFIG"
else
    mkdir -p "$(dirname "$CONFIG")"
    cat > "$CONFIG" <<EOF
model_path=$DESTDIR/$MODEL_NAME
classifier=onnx
EOF
    chmod 644 "$CONFIG"
    echo "Created new config at $CONFIG"
fi

echo "Model installation complete."
