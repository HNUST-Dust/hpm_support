.. _ethernet:

Echo Server
============
`zephyr sample link <https://docs.zephyrproject.org/3.7.0/samples/net/sockets/echo_server/README.html>`_

Path
---------------

.. code-block::

    zephyr/samples/net/sockets/echo_server

Build Cmd
-----------

As dust-hpm6750 for example:

.. code-block:: console

    west build -p always -b dust-hpm6750 -S ethernet zephyr/samples/net/sockets/echo_server