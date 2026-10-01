#!/bin/bash

# Script to run plugin measurements with multiple signal types
#
# This script runs measurements for sine, noise, and sweep signals with
# configurable parameter ranges and fixed values.
#
# Parameter Ranges:
#   - air: 0-10 (Linear, 3 buckets)
#   - bass ++: 0-10 (Linear, 3 buckets)
#   - clipper: 0-1 (Linear, 3 buckets)
#   - input: -24, 0, 24 dB (ExplicitValues)
#   - output: -24, 0, 24 dB (ExplicitValues)
#   - pre: -24, 0, 24 dB (ExplicitValues)
#   - spread: m, c, s (mapped to 0.0, 0.5, 1.0 normalized)
#
# Fixed Parameters (set but not varied in grid):
#   - air ++ freq, bass ++ freq, d/e/a/n, m/i/k/e/o/ob/b
#   To set fixed values, uncomment and edit FIXED_PARAMS array below.
#   Format: "paramName:normalizedValue" (0.0-1.0)
#
# Usage: ./run_measurements.sh [--plugin PATH] [--output DIR] [--params PARAM1,PARAM2,...]

set -e  # Exit on error

# Default values
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_ANALYSER_DIR="$SCRIPT_DIR"
CLI_EXECUTABLE="$PLUGIN_ANALYSER_DIR/build-release/plugin_measure_grid_cli"
DEFAULT_PLUGIN_PATH="/Volumes/External SSD/Plug-Ins/VST3/Acustica/GAINSTATION2.vst3"
DEFAULT_OUTPUT_DIR="/Volumes/External SSD/plugin_measure_output"
TEMP_CONFIG_DIR="$PLUGIN_ANALYSER_DIR/temp_configs"

# Configuration
SAMPLE_RATE=48000
DURATION_SECONDS=5
BLOCK_SIZE=256
NUM_BUCKETS=3
INPUT_GAIN_BUCKETS=(-24.0 0.0 24.0)

# Signal type configurations
SINE_FREQ=1000.0
SWEEP_START=20.0
SWEEP_END=20000.0

# Parameter configuration
# Main parameters to vary in the measurement grid
SELECTED_PARAMS=(
    "air"           # Range: 0-10 (Linear, 3 buckets)
    "bass ++"       # Range: 0-10 (Linear, 3 buckets)
    "clipper"       # Range: 0-1 (Linear, 3 buckets) - adjust if different
    "input"         # Range: -24, 0, 24 dB (ExplicitValues)
    "output"        # Range: -24, 0, 24 dB (ExplicitValues)
    "pre"           # Range: -24, 0, 24 dB (ExplicitValues)
    "spread"        # Range: m, c, s (ExplicitValues: 0.0, 0.5, 1.0 normalized)
    "d/e/a/n"       # Range: 0-1 (Linear, 4 buckets)
    "m/i/k/e/o/ob/b"  # Range: 0-1 (Linear, 4 buckets)
)

# Fixed-value parameters (set to specific values, not varied in grid)
# Format: "paramName:value" where value is normalized (0.0-1.0)
# These will be set but not included in the measurement grid
FIXED_PARAMS=(
    # "air ++ freq:0.5"      # Set to normalized value 0.5 (adjust as needed)
    # "bass ++ freq:0.5"      # Set to normalized value 0.5 (adjust as needed)
    # "d /e / a / n:0.0"     # Set to first option (adjust as needed)
    # "m / i / k / e / o / ob / b:0.0"  # Set to first option (adjust as needed)
)

# Parse command line arguments
PLUGIN_PATH="$DEFAULT_PLUGIN_PATH"
OUTPUT_DIR="$DEFAULT_OUTPUT_DIR"
CUSTOM_PARAMS=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --plugin)
            PLUGIN_PATH="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --params)
            CUSTOM_PARAMS="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--plugin PATH] [--output DIR] [--params PARAM1,PARAM2,...]"
            exit 1
            ;;
    esac
done

# Parse custom params if provided
if [[ -n "$CUSTOM_PARAMS" ]]; then
    IFS=',' read -ra SELECTED_PARAMS <<< "$CUSTOM_PARAMS"
    echo "Using custom parameters: ${SELECTED_PARAMS[*]}"
fi

# Check if CLI executable exists
if [[ ! -f "$CLI_EXECUTABLE" ]]; then
    echo "❌ Error: CLI executable not found: $CLI_EXECUTABLE"
    echo "   Run 'make build-release' first"
    exit 1
fi

# Check if plugin exists
if [[ ! -f "$PLUGIN_PATH" ]] && [[ ! -d "$PLUGIN_PATH" ]]; then
    echo "❌ Error: Plugin not found: $PLUGIN_PATH"
    exit 1
fi

# Create temp config directory
mkdir -p "$TEMP_CONFIG_DIR"

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "=========================================="
echo "  Plugin Measurement Runner"
echo "=========================================="
echo "Plugin: $PLUGIN_PATH"
echo "Output: $OUTPUT_DIR"
echo "Parameters: ${SELECTED_PARAMS[*]:-(none selected - edit script to add)}"
echo "Buckets per param: $NUM_BUCKETS"
echo ""

# Check if any parameters are selected
if [[ ${#SELECTED_PARAMS[@]} -eq 0 ]]; then
    echo "⚠️  Warning: No parameters selected!"
    echo "   Edit the SELECTED_PARAMS array in this script to add parameters."
    echo "   Configured parameters:"
    echo "     - air: 0-10 (Linear, 3 buckets)"
    echo "     - bass ++: 0-10 (Linear, 3 buckets)"
    echo "     - clipper: 0-1 (Linear, 3 buckets)"
    echo "     - input: -24, 0, 24 dB (ExplicitValues)"
    echo "     - output: -24, 0, 24 dB (ExplicitValues)"
    echo "     - pre: -24, 0, 24 dB (ExplicitValues)"
    echo "     - spread: m, c, s (ExplicitValues: 0.0, 0.5, 1.0)"
    echo ""
    echo "   Fixed parameters (set but not varied):"
    echo "     - air ++ freq, bass ++ freq, d/e/a/n, m/i/k/e/o/ob/b"
    echo "     Edit FIXED_PARAMS array to set their values"
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Function to get parameter bucket config based on parameter name
get_param_bucket_config() {
    local param_name=$1
    local param_escaped=$(echo "$param_name" | sed 's/"/\\"/g')

    case "$param_name" in
        "air")
            echo "    {
      \"paramName\": \"$param_escaped\",
      \"strategy\": \"Linear\",
      \"min\": 0.0,
      \"max\": 10.0,
      \"numBuckets\": $NUM_BUCKETS
    }"
            ;;
        "bass ++")
            echo "    {
      \"paramName\": \"$param_escaped\",
      \"strategy\": \"Linear\",
      \"min\": 0.0,
      \"max\": 10.0,
      \"numBuckets\": $NUM_BUCKETS
    }"
            ;;
        "clipper")
            echo "    {
      \"paramName\": \"$param_escaped\",
      \"strategy\": \"Linear\",
      \"min\": 0.0,
      \"max\": 1.0,
      \"numBuckets\": $NUM_BUCKETS
    }"
            ;;
        "input"|"output"|"pre")
            echo "    {
      \"paramName\": \"$param_escaped\",
      \"strategy\": \"ExplicitValues\",
      \"values\": [-24.0, 0.0, 24.0]
    }"
            ;;
        "spread")
            # Map m=0.0, c=0.5, s=1.0 (normalized values)
            echo "    {
      \"paramName\": \"$param_escaped\",
      \"strategy\": \"ExplicitValues\",
      \"values\": [0.0, 0.5, 1.0]
    }"
            ;;
        "d/e/a/n")
            # 4 buckets for d/e/a/n
            echo "    {
      \"paramName\": \"$param_escaped\",
      \"strategy\": \"Linear\",
      \"min\": 0.0,
      \"max\": 1.0,
      \"numBuckets\": 4
    }"
            ;;
        "m/i/k/e/o/ob/b")
            # 4 buckets for m/i/k/e/o/ob/b
            echo "    {
      \"paramName\": \"$param_escaped\",
      \"strategy\": \"Linear\",
      \"min\": 0.0,
      \"max\": 1.0,
      \"numBuckets\": 4
    }"
            ;;
        *)
            # Default: Linear 0-1
            echo "    {
      \"paramName\": \"$param_escaped\",
      \"strategy\": \"Linear\",
      \"min\": 0.0,
      \"max\": 1.0,
      \"numBuckets\": $NUM_BUCKETS
    }"
            ;;
    esac
}

# Function to generate config JSON for a signal type
generate_config() {
    local signal_type=$1
    local config_file=$2

    cat > "$config_file" <<EOF
{
  "pluginPath": "$PLUGIN_PATH",
  "sampleRate": $SAMPLE_RATE,
  "seconds": $DURATION_SECONDS,
  "blockSize": $BLOCK_SIZE,
  "signalType": "$signal_type",
  "sineFrequency": $SINE_FREQ,
  "sweepStartHz": $SWEEP_START,
  "sweepEndHz": $SWEEP_END,
  "inputGainBucketsDb": [$(IFS=','; echo "${INPUT_GAIN_BUCKETS[*]}")],
  "parameterBuckets": [
EOF

    # Add parameter buckets for selected parameters
    local first=true
    for param in "${SELECTED_PARAMS[@]}"; do
        if [[ "$first" == true ]]; then
            first=false
        else
            echo "," >> "$config_file"
        fi

        get_param_bucket_config "$param" >> "$config_file"
    done

    # Add fixed parameters (as single-value ExplicitValues so they're set but don't expand the grid)
    for fixed_param in "${FIXED_PARAMS[@]}"; do
        if [[ -n "$fixed_param" ]]; then
            IFS=':' read -r fixed_name fixed_value <<< "$fixed_param"
            if [[ -n "$fixed_name" && -n "$fixed_value" ]]; then
                echo "," >> "$config_file"
                local fixed_escaped=$(echo "$fixed_name" | sed 's/"/\\"/g')
                cat >> "$config_file" <<EOF
    {
      "paramName": "$fixed_escaped",
      "strategy": "ExplicitValues",
      "values": [$fixed_value]
    }
EOF
            fi
        fi
    done

    # Determine analyzers based on signal type
    local analyzers=""
    case "$signal_type" in
        sine)
            analyzers='"RawCsv", "RmsPeak", "TransferCurve", "Thd"'
            ;;
        noise|sweep)
            analyzers='"RawCsv", "RmsPeak", "TransferCurve", "LinearResponse"'
            ;;
        *)
            analyzers='"RawCsv", "RmsPeak", "TransferCurve"'
            ;;
    esac

    cat >> "$config_file" <<EOF
  ],
  "analyzers": [$analyzers]
}
EOF
}

# Function to run measurement for a signal type
run_measurement() {
    local signal_type=$1
    local config_file="$TEMP_CONFIG_DIR/config_${signal_type}.json"
    local signal_output_dir="$OUTPUT_DIR/${signal_type}"

    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  Running measurement: $signal_type"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Config: $config_file"
    echo "Output: $signal_output_dir"
    echo ""

    # Generate config
    generate_config "$signal_type" "$config_file"

    # Create output directory for this signal type
    mkdir -p "$signal_output_dir"

    # Run CLI (using all available CPU cores for parallelization)
    "$CLI_EXECUTABLE" \
        --config "$config_file" \
        --out "$signal_output_dir" \
        --plugin "$PLUGIN_PATH"

    if [[ $? -eq 0 ]]; then
        echo "✅ $signal_type measurement complete!"
    else
        echo "❌ $signal_type measurement failed!"
        return 1
    fi
}

# Calculate estimated time
calculate_time() {
    local num_params=${#SELECTED_PARAMS[@]}
    if [[ $num_params -eq 0 ]]; then
        num_params=1  # Avoid division by zero
    fi

    # Calculate combinations more accurately based on actual bucket counts
    local combinations=1
    for param in "${SELECTED_PARAMS[@]}"; do
        local lower_param=$(echo "$param" | tr '[:upper:]' '[:lower:]')
        if [[ "$lower_param" == "input" || "$lower_param" == "output" || "$lower_param" == "pre" ]]; then
            combinations=$((combinations * 3))  # ExplicitValues: -24, 0, 24
        elif [[ "$lower_param" == "spread" ]]; then
            combinations=$((combinations * 3))  # ExplicitValues: 0.0, 0.5, 1.0
        elif [[ "$lower_param" == "d/e/a/n" ]]; then
            combinations=$((combinations * 4))  # 4 buckets
        elif [[ "$lower_param" == "m/i/k/e/o/ob/b" ]]; then
            combinations=$((combinations * 4))  # 4 buckets
        else
            combinations=$((combinations * 3))  # Default 3 buckets
        fi
    done
    combinations=$((combinations * ${#INPUT_GAIN_BUCKETS[@]}))
    local total_runs=$((combinations * 3))  # 3 signal types

    # Estimate time: assume ~1 second per run, divided by number of CPU cores (8)
    local cores=$(sysctl -n hw.ncpu 2>/dev/null || echo "8")
    local estimated_minutes=$((total_runs / 60 / cores))
    local estimated_hours=$((estimated_minutes / 60))
    local estimated_mins=$((estimated_minutes % 60))

    echo "Estimated total runs: $total_runs"
    echo "Estimated time: ~${estimated_hours}h ${estimated_mins}m (with $cores parallel threads)"
}

# Show summary
echo "Measurement Plan:"
echo "  Signal types: sine, noise, sweep"
echo "  Parameters: ${#SELECTED_PARAMS[@]}"
calculate_time
echo ""

read -p "Start measurements? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelled."
    exit 0
fi

# Run measurements for each signal type
START_TIME=$(date +%s)

run_measurement "sine" || echo "⚠️  Sine measurement had errors"
run_measurement "noise" || echo "⚠️  Noise measurement had errors"
run_measurement "sweep" || echo "⚠️  Sweep measurement had errors"

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
ELAPSED_MIN=$((ELAPSED / 60))
ELAPSED_SEC=$((ELAPSED % 60))

echo ""
echo "=========================================="
echo "  All Measurements Complete!"
echo "=========================================="
echo "Total time: ${ELAPSED_MIN}m ${ELAPSED_SEC}s"
echo "Output directory: $OUTPUT_DIR"
echo ""
echo "Results:"
echo "  - sine:  $OUTPUT_DIR/sine/"
echo "  - noise: $OUTPUT_DIR/noise/"
echo "  - sweep: $OUTPUT_DIR/sweep/"
echo ""

# Cleanup temp configs
echo "Cleaning up temporary configs..."
rm -rf "$TEMP_CONFIG_DIR"

echo "✅ Done!"
