.. _shell_module:

Shell Module
=============
`zephyr sample link <https://docs.zephyrproject.org/3.7.0/samples/subsys/shell/shell_module/README.html>`_

Overview
--------

This is a simple application demonstrating how to write and register commands
using the Shell API:

- **Register Static commands**: ``version`` is a static command that prints the kernel version.
- **Register Dynamic commands**: See ``dynamic`` command for details on how dynamic commands are implemented.
- **Register Dictionary commands**: ``dictionary`` implements subsect of dictionary commands.
- **Set a Bypass callback**: ``bypass`` implements the bypass callback.

Path
---------------

.. code-block::

    zephyr/samples/subsys/shell/shell_module

Build Cmd
-----------

As dust-hpm6750 for example:

.. code-block:: console

    west build -p always -b dust-hpm6750 -S shell_module zephyr/samples/subsys/shell/shell_module
