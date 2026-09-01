.. _mass:

mass
==========
`zephyr sample link <https://docs.zephyrproject.org/3.7.0/samples/subsys/usb/mass/README.html>`_

Sample Path
---------------

.. code-block::

    zephyr/samples/subsys/usb/mass

Build Cmd
------------

As dust-hpm6750 for example:

.. code-block:: console

    west build -p always -b dust-hpm6750 -S mass samples/subsys/usb/mass -T sample.usb_device_next.mass_ram_none
