"""Bounded projection algebra; does not identify a unique world camera."""
import argparse
from fractions import Fraction
import json
from pathlib import Path
import numpy as np

from binary32_oracle import fraction, rounded_bits, evaluate
from transform_context_inspect import inspect_context
from transform_store_inspect import require


def dot_words(columns, vector):
    # Mathematical dot reference, not a complete binary64 instruction emulator.
    return [rounded_bits(sum((fraction(columns[j][i]) * fraction(vector[j])
                              for j in range(4)), Fraction(0)), 3) for i in range(4)]


def factor(matrix):
    require(matrix.shape == (4, 4) and np.isfinite(matrix).all(), 'invalid matrix')
    require(np.array_equal(matrix[3], [0, 0, 0, 1]), 'not captured affine layout')
    scales = np.linalg.norm(matrix[:3, :3], axis=1)
    require(np.all(scales > 0), 'degenerate axes')
    axes = matrix[:3, :3] / scales[:, None]
    residual = float(np.max(np.abs(axes @ axes.T - np.eye(3))))
    require(residual < 1e-6 and np.linalg.det(axes) > 0, 'not scaled orthogonal axes')
    calibration = np.diag([*scales, 1])
    composite = np.linalg.solve(calibration, matrix)
    require(np.max(np.abs(calibration @ composite - matrix)) < 1e-9, 'factorization mismatch')
    return calibration, composite, residual


def project(matrix, point):
    p = matrix @ point
    require(abs(p[2]) > 1e-9, 'projection singularity')
    return np.array([320 + p[0]/p[2], 240 + p[1]/p[2]])


def analyze(columns, vector, output):
    require(len(columns) == 4 and all(len(c) == 4 for c in columns)
            and len(vector) == len(output) == 4, 'invalid captured dimensions')
    require(dot_words(columns, vector) == output, 'exact-dot reference differs from captured transform')
    require(dot_words(list(map(list, zip(*columns))), vector) != output,
            'wrong-layout control did not fail')
    matrix = np.array([[float(fraction(v)) for v in c] for c in columns]).T
    point = np.array([float(fraction(v)) for v in vector])
    calibration, composite, residual = factor(matrix)
    expected = project(matrix, point)
    require(np.max(np.abs(project(calibration @ composite, point)-expected)) < 1e-9,
            'factorized projection mismatch')
    # Construct a distinct model/view split: H is a rigid change of local/world
    # basis. Compensating it in the view leaves the complete mapping unchanged.
    h = np.array([[0., -1., 0., 3.], [1., 0., 0., -2.],
                  [0., 0., 1., 5.], [0., 0., 0., 1.]])
    alternative_view = composite @ np.linalg.inv(h)
    probes = np.vstack([point, point + [0.125, 0, 0, 0],
                        point + [0, 0.125, 0, 0], point + [0, 0, 0.125, 0]])
    ambiguity_error = max(float(np.max(np.abs(project(calibration @ alternative_view @ h, p)
                                                - project(matrix, p)))) for p in probes)
    require(ambiguity_error < 1e-9, 'equivalent decomposition changed projection')
    wrong_scale = calibration.copy()
    wrong_scale[0, 0] *= 2
    scale_error = float(np.max(np.abs(project(wrong_scale @ composite, point)-expected)))
    decomposition_error = float(np.max(np.abs(project(calibration @ alternative_view, point)-expected)))
    require(scale_error > 1 and decomposition_error > 1, 'wrong scale/decomposition controls did not fail')
    return dict(axis_scales=np.diag(calibration)[:3].tolist(), orthogonality_residual=residual,
                algebraic_projection=expected.tolist(), wrong_scale_pixel_error=scale_error,
                wrong_decomposition_pixel_error=decomposition_error,
                equivalent_decomposition_error=ambiguity_error, probe_points=len(probes),
                unique_model_view=False, world_camera='unproven', physical_scale='unproven')


def inspect(path):
    witness = inspect_context(path)
    source = witness['ta']['derived']['source']
    columns = [[int(v, 16) for v in c.split(',')] for c in source[4]]
    vector = [int(v, 16) for v in source[3].split(',')]
    result = analyze(columns, vector, source[1])
    reciprocal = evaluate('div', 0x3f800000, source[1][2], 0, 3)
    xyz = [evaluate('madd', 0x43a00000, source[1][0], reciprocal, 3, True),
           evaluate('madd', 0x43700000, source[1][1], reciprocal, 3, True),
           evaluate('mul', 0x3f851eb8, reciprocal, 0, 3)]
    require(xyz == witness['ta']['packet'][1:4], 'rounded projection differs from decoded vertex')
    result.update(decoded_xyz_words=[f'{v:08x}' for v in xyz],
                  scope='one-linked-composite-transform-algebra', exact_dot_matches=True,
                  binary64_instruction_emulation=False, final_frame_ownership='unproven')
    return result


def analyze_opaque(columns, vector, output, center, decoded):
    """The linked opaque path uses separate MUL/ADD and unscaled reciprocal Z.

    Reuse the factorizer, but never import the translucent fixture's fused
    projection or depth multiplier. This remains one observed sample.
    """
    require(center == [0x43a00000, 0x43700000], 'unsupported observed viewport center')
    result = analyze(columns, vector, output)
    require(fraction(output[2]) > 0, 'opaque sample is not positive-depth geometry')
    reciprocal = evaluate('div', 0x3f800000, output[2], 0, 3)
    xyz = [evaluate('madd', center[i], output[i], reciprocal, 3, False) for i in range(2)]
    xyz.append(reciprocal)
    require(xyz == decoded, 'opaque split projection differs from decoded vertex')
    borrowed_depth = evaluate('mul', 0x3f851eb8, reciprocal, 0, 3)
    require(borrowed_depth != decoded[2], 'wrong borrowed depth-scale control did not fail')
    forced_unit = list(vector)
    forced_unit[3] = 0x3f800000
    unit_control = None
    if vector[3] != forced_unit[3]:
        unit_control = dot_words(columns, forced_unit) != output
        require(unit_control, 'nonunit-W control is not discriminating for this sample')
    scales = result['axis_scales']
    normalized = [scales[0]/scales[2], scales[1]/scales[2]]
    result.update(
        normalized_projection_scales=normalized,
        decoded_xyz_words=[f'{v:08x}' for v in xyz],
        actual_w_word=f'{vector[3]:08x}', nonunit_w_control_rejected=unit_control,
        reciprocal_depth_scale_word='3f800000', screen_offset=[320, 240],
        screen_arithmetic='rounded-reciprocal-then-separate-multiply-add',
        wrong_translucent_depth_scale_rejected=True,
        camera_relative_point=[float(fraction(output[0]))/normalized[0],
                               float(fraction(output[1]))/normalized[1],
                               float(fraction(output[2]))],
        scope='one-linked-opaque-transform-algebra', observed_points=1,
        synthetic_probe_points=3, shared_game_camera=False,
        usable_camera_contract=False, production_enabled=False,
        exact_dot_matches=True, binary64_instruction_emulation=False)
    return result


def inspect_opaque(path):
    # Import lazily: the historical translucent CLI keeps its original contract.
    from initial_edge_inspect import inspect as inspect_initial_edge
    from record_calc_inspect import inspect_block
    from transform_store_inspect import load_session

    frame = path/'frame-001782'
    def read(name):
        file = frame/name
        require(file.stat().st_size <= 8*1024*1024, 'opaque JSON bound')
        return json.loads(file.read_text(encoding='utf-8'))
    scene, manifest = read('pvr-scene.json'), read('manifest.json')
    session = load_session(path)
    edge = inspect_initial_edge(session, scene, manifest)
    final = inspect_block(session)
    columns = [edge['xf_matrix_words'][i:i+4] for i in range(0, 16, 4)]
    result = analyze_opaque(columns, edge['source_point_words'], edge['transformed_words'],
                            [final['live_input_words'][i] for i in (20, 21)],
                            scene['vertices'][4][:3])
    result.update(frame_id=manifest['frame_id'], game_id=manifest['game_id'],
                  producer_identity=manifest['producer_identity'], vertex=4,
                  prior_source_to_decoded_chain_verified=True)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--capture', type=Path, help='Historical linked translucent fixture')
    group.add_argument('--opaque-capture', type=Path, help='Linked initial-supply opaque fixture')
    args = parser.parse_args()
    try:
        result = inspect(args.capture) if args.capture else inspect_opaque(args.opaque_capture)
        print(json.dumps(result, indent=2))
    except (ValueError, OSError, KeyError, IndexError, TypeError) as error:
        parser.exit(1, f'Transform semantics rejected: {error}\n')
