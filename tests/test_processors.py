import numpy as np

from realtime_dsp.processors import NLSMProcessor, PassthroughProcessor


def test_passthrough_preserves_values_and_shape():
    block = np.arange(16, dtype=np.float32).reshape(8, 2)
    result = PassthroughProcessor().Process(block)
    np.testing.assert_array_equal(result, block)
    assert result is not block


def test_nlsm_without_reference_is_safe_passthrough():
    block = np.random.default_rng(1).normal(size=(32, 1)).astype(np.float32)
    result = NLSMProcessor(filter_length=8).Process(block)
    np.testing.assert_allclose(result, block)


def test_nlsm_reduces_known_echo_after_adaptation():
    rng = np.random.default_rng(2)
    reference = rng.normal(size=(800, 1)).astype(np.float32)
    mic = np.zeros_like(reference)
    mic[1:] = 0.6 * reference[:-1]
    processor = NLSMProcessor(filter_length=8, step_size=0.8)
    output = []
    for start in range(0, len(mic), 32):
        end = start + 32
        processor.set_reference(reference[start:end])
        output.append(processor.Process(mic[start:end]))
    error = np.concatenate(output)[200:]
    assert np.mean(error**2) < np.mean(mic[200:] ** 2)
