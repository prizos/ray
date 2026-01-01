# Ray - Game Client
# Meson build configuration

project('ray', 'c',
  version: '1.0.0',
  default_options: [
    'c_std=c99',
    'warning_level=2',
  ],
)

# Include directories
inc = include_directories('include')

# Math library
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

# Find raylib dependency
raylib_dep = dependency('raylib', required: false)
if not raylib_dep.found()
  # Try pkg-config explicitly
  raylib_dep = dependency('raylib', method: 'pkg-config', required: true)
endif

# Platform-specific dependencies
platform_deps = []
if host_machine.system() == 'darwin'
  platform_deps += dependency('appleframeworks', modules: ['OpenGL', 'Cocoa', 'IOKit', 'CoreVideo'])
elif host_machine.system() == 'linux'
  platform_deps += dependency('gl')
  platform_deps += dependency('threads')
  platform_deps += dependency('x11')
  platform_deps += cc.find_library('dl', required: false)
  platform_deps += cc.find_library('rt', required: false)
endif

# All dependencies combined
all_deps = [raylib_dep, m_dep] + platform_deps

# Build option for debug metrics
if get_option('debug_metrics')
  add_project_arguments('-DDEBUG_METRICS', language: 'c')
endif

# ============ SOURCE FILES ============

# Core chunk/physics system
chunk_sources = files(
  'src/chunk.c',
  'src/chunk_world.c',
  'src/chunk_physics.c',
)

# Terrain and noise generation
terrain_sources = files(
  'src/terrain.c',
  'src/noise.c',
)

# Tree system (space colonization algorithm)
tree_sources = files(
  'src/tree.c',
  'src/octree.c',
  'src/attractor_octree.c',
)

# Game sources
game_sources = files(
  'src/main.c',
  'src/game.c',
  'src/render.c',
  'src/beaver.c',
  'src/debug_metrics.c',
)

# All main game sources
all_game_sources = chunk_sources + terrain_sources + tree_sources + game_sources

# ============ MAIN EXECUTABLE ============

game_exe = executable('game',
  all_game_sources,
  include_directories: inc,
  dependencies: all_deps,
  install: true,
)

# ============ TERRAIN TUNING TOOL ============

terrain_tune_sources = files('src/terrain_tune.c') + terrain_sources + tree_sources

terrain_tune_exe = executable('terrain_tune',
  terrain_tune_sources,
  include_directories: inc,
  dependencies: all_deps,
  install: false,
)

# ============ TESTS ============

# Common test sources (shared between most tests)
# Include debug_metrics.c for DEBUG_METRICS support in tests
common_test_sources = chunk_sources + terrain_sources + tree_sources + files('src/debug_metrics.c')

# Tests that need full common sources
full_tests = {
  'test_svo_unit': files('tests/test_svo_unit.c'),
  'test_svo_physics': files('tests/test_svo_physics.c'),
  'test_3d_physics': files('tests/test_3d_physics.c'),
  'test_energy_conservation': files('tests/test_energy_conservation.c'),
  'test_tool_integration': files('tests/test_tool_integration.c'),
  'test_e2e_simulation': files('tests/test_e2e_simulation.c'),
  'test_physics_predictions': files('tests/test_physics_predictions.c'),
}

foreach name, sources : full_tests
  exe = executable(name,
    sources + common_test_sources,
    include_directories: inc,
    dependencies: all_deps,
  )
  # test_3d_physics has intensive fluid dynamics tests that need more time
  if name == 'test_3d_physics'
    test(name, exe, timeout: 120)
  else
    test(name, exe)
  endif
endforeach

# Tests with minimal dependencies
# test_phase_specific_cp only needs chunk.c
test_phase_specific_cp_exe = executable('test_phase_specific_cp',
  files('tests/test_phase_specific_cp.c', 'src/chunk.c'),
  include_directories: inc,
  dependencies: all_deps,
)
test('test_phase_specific_cp', test_phase_specific_cp_exe)

# test_noise only needs noise.c
test_noise_exe = executable('test_noise',
  files('tests/test_noise.c', 'src/noise.c'),
  include_directories: inc,
  dependencies: all_deps,
)
test('test_noise', test_noise_exe)

# test_octree only needs octree.c
test_octree_exe = executable('test_octree',
  files('tests/test_octree.c', 'src/octree.c'),
  include_directories: inc,
  dependencies: all_deps,
)
test('test_octree', test_octree_exe)
