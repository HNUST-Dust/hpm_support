.. _can:

CAN (Enable CAN-FD As Default)
==================================

Path
---------------

.. code-block::

    zephyr/tests/drivers/can/api
    zephyr/tests/drivers/can/timing
    zephyr/tests/drivers/can/shell

Build Cmd
-----------

As dust-hpm6750 for example:

.. code-block:: console

    west build -p always -b dust-hpm6750 -S can tests/drivers/can/api
    west build -p always -b dust-hpm6750 -S can tests/drivers/can/timing
    west build -p always -b dust-hpm6750 -S can tests/drivers/can/shell