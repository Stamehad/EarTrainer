"""EarTrainer package initialization.

Provides access to the compiled adaptive drills engine as soon as the package
is imported, so notebooks and applications can simply `import eartrainer`.
"""

from __future__ import annotations

import importlib
import importlib.util
import sys
from pathlib import Path
from types import ModuleType
from typing import Iterable, Optional

__version__ = "0.1.0"

__all__ = ["__version__"]

_CPP_MODULE_NAME = "eartrainer.eartrainer_Cpp.python.eartrainer"
_cpp_module: Optional[ModuleType] = None
_cpp_import_error: Optional[ModuleNotFoundError] = None


def _bootstrap_cpp_api() -> None:
    """Load the python façade for the C++ adaptive drills bindings.

    Populates the current module's globals with the symbols exposed by the shim
    package so callers can import directly from `eartrainer`.
    """

    global _cpp_module, _cpp_import_error

    if _cpp_module is not None or _cpp_import_error is not None:
        return

    try:
        module = importlib.import_module(_CPP_MODULE_NAME)
    except ModuleNotFoundError as exc:
        shim_root = Path(__file__).resolve().parent / "eartrainer_Cpp" / "python" / "eartrainer"
        init_file = shim_root / "__init__.py"
        if not init_file.exists():
            _cpp_import_error = exc
            return

        spec = importlib.util.spec_from_file_location(
            "eartrainer._cpp_shim",
            init_file,
            submodule_search_locations=[str(shim_root)],
        )
        if spec is None or spec.loader is None:
            _cpp_import_error = exc
            return

        module = importlib.util.module_from_spec(spec)
        sys.modules.setdefault("eartrainer._cpp_shim", module)
        try:
            spec.loader.exec_module(module)
        except ModuleNotFoundError as shim_exc:
            _cpp_import_error = shim_exc
            return
    else:
        sys.modules.setdefault("eartrainer._cpp_shim", module)

    _cpp_module = module
    _export_from_cpp(module, getattr(module, "__all__", ()))
    _ensure_core_aliases()


def _export_from_cpp(module: ModuleType, names: Iterable[str]) -> None:
    """Populate this module's globals with the given names from the shim."""

    for name in names:
        globals()[name] = getattr(module, name)
        if name not in __all__:
            __all__.append(name)


def _ensure_core_aliases() -> None:
    """Ensure the compiled module is registered under all expected names."""

    core_aliases = [
        "eartrainer.eartrainer_Cpp._earcore",
        "eartrainer._cpp_shim._earcore",
    ]

    present = [(alias, sys.modules.get(alias)) for alias in core_aliases]
    module_obj = next((mod for _, mod in present if mod is not None), None)

    if module_obj is None:
        return

    for alias, module in present:
        if module is None:
            sys.modules[alias] = module_obj


# Attempt to bootstrap immediately so the common case works without delay.
_bootstrap_cpp_api()


def __getattr__(name: str):
    """Lazily expose shim attributes while providing helpful error messages."""

    if name in globals():
        return globals()[name]

    if name.startswith("_"):
        raise AttributeError(name)

    _bootstrap_cpp_api()
    if _cpp_module is not None and name in globals():
        return globals()[name]

    if _cpp_import_error is not None:
        raise AttributeError(
            f"{name!r} is unavailable because the adaptive drills extension "
            "is not built. Run eartrainer/eartrainer_Cpp/build_and_install.sh "
            "to compile the C++ core."
        ) from _cpp_import_error

    raise AttributeError(name)
