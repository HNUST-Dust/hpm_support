.. _blinky_pwm:

PWM Blinky
============
`zephyr sample link <https://docs.zephyrproject.org/3.7.0/samples/basic/blinky_pwm/README.html>`_

Path
---------------

.. code-block::

    zephyr/samples/basic/blinky_pwm

Build Cmd
-----------

As dust-hpm6750 for example:

.. code-block:: console

    west build -p always -b dust-hpm6750 -S blinky_pwm zephyr/samples/basic/blinky_pwm

Note
------

- The light on the board is not connected to the PWM pin and requires an external component(light/oscilloscope).