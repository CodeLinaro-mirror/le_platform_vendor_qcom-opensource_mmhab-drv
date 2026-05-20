.. SPDX-License-Identifier: GPL-2.0-only
..
.. Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

:tocdepth: 3

MMHAB Public API Reference Manual
=================================

**Version:** ``v1.0`` (2026-04-23)

This page documents public MMHAB APIs for:

* Linux HAB clients in kernel space (PVM and GVM).
* Linux HAB clients in user space (PVM and GVM).
* QNX HAB clients.

All APIs return ``0`` on success and a negative ``errno`` on failure.

.. note::

   Throughout this document, return values prefixed with ``+`` (for example
   ``+EINVAL``, ``+EBUSY``) denote positive error codes propagated from a QNX
   subsystem rather than from the HAB driver itself.
   These are distinct from the driver's own negative ``-errno`` return values.


HAB MMID
--------

MMID (Multimedia ID) is a predefined HAB client identifier.
MMIDs are defined in:

* ``include/uapi/linux/habmmid.h`` in Linux
* ``habmmid.h`` in QNX

and are used by ``habmm_socket_open()`` to select HAB routing. It consists of
major mmid and minor mmid.

MMID bit layout and decode
^^^^^^^^^^^^^^^^^^^^^^^^^^

MMID bitmap (bit 31 on the left)::

   31                      24 23                      16 15                      0
   +-------------------------+--------------------------+-------------------------+
   | reserved (must be 0)    | minor MMID (8 bits)     | major MMID (16 bits)    |
   +-------------------------+--------------------------+-------------------------+

One **major MMID** maps to one physical channel in the HAB driver.
Minor MMIDs are encoded for sub-identification within the same major domain,
so multiple logical HAB clients can be differentiated while sharing the same
major-domain physical channel class. Minor MMID is usually used when multiple
threads open channels under the same major MMID.

A HAB channel is created (and ``habmm_socket_open()`` returns success) only
when **both** major and minor MMID values match between the two VM endpoints.

This table lists the predefined **major MMID values** (client IDs) from
``habmmid.h``.
``HAB_MMID_CREATE(major, minor)`` encodes full MMID values as
``(major & 0xFFFF) | ((minor & 0xFF) << 16)``.

.. _hab-mmid-mapping-table:

MMID mapping table
^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Group
     - Defined MMIDs
   * - ``AUD``
     - | ``MM_AUD_1=101``
       | ``MM_AUD_2=102``
       | ``MM_AUD_3=103``
       | ``MM_AUD_4=104``
   * - ``CAM``
     - | ``MM_CAM_1=201``
       | ``MM_CAM_2=202``
   * - ``DISP``
     - | ``MM_DISP_1=301``
       | ``MM_DISP_2=302``
       | ``MM_DISP_3=303``
       | ``MM_DISP_4=304``
       | ``MM_DISP_5=305``
   * - ``GFX``
     - | ``MM_GFX=401``
   * - ``VID``
     - | ``MM_VID=501``
       | ``MM_VID_2=502``
       | ``MM_VID_3=503``
   * - ``MISC``
     - | ``MM_MISC=601``
   * - ``QCPE``
     - | ``MM_QCPE_VM1=701``
   * - ``CLK``
     - | ``MM_CLK_VM1=801``
       | ``MM_CLK_VM2=802``
   * - ``FDE``
     - | ``MM_FDE_1=901``
   * - ``BUFFERQ``
     - | ``MM_BUFFERQ_1=1001``
   * - ``DATA``
     - | ``MM_DATA_NETWORK_1=1101``
       | ``MM_DATA_NETWORK_2=1102``
   * - ``HSI2S``
     - | ``MM_HSI2S_1=1201``
   * - ``XVM``
     - | ``MM_XVM_1=1301``
       | ``MM_XVM_2=1302``
       | ``MM_XVM_3=1303``
   * - ``VNW``
     - | ``MM_VNW_1=1401``
   * - ``EXT``
     - | ``MM_EXT_1=1501``
       | ``MM_EXT_2=1502``
       | ``MM_EXT_3=1503``
   * - ``GPCE``
     - | ``MM_GPCE_1=1601``
   * - ``SOCCP``
     - | ``MM_SOCCP_1=1701``
   * - ``DPRX``
     - | ``MM_DPRX_1=1801``
       | ``MM_DPRX_2=1802``
   * - ``EVA``
     - | ``MM_EVA_1=1901``


.. _hab-osid-mapping-table:

HAB OSID mapping table
----------------------

HAB OSID (OS Identifier) is a HAB driver-defined identifier that distinguishes
between the different virtual machines and the QNX host in HAB communication.
It is assigned and maintained by the HAB driver.

.. list-table::
   :header-rows: 1

   * - HAB OSID
     - Meaning
   * - ``0``
     - ``Linux PVM / QNX Host``
   * - ``1``
     - ``Reserved / unassigned in current setups``
   * - ``2``
     - ``1st LA GVM``
   * - ``3``
     - ``1st LV GVM``
   * - ``4``
     - ``Reserved / unassigned in current setups``
   * - ``5``
     - ``2nd LA GVM (LA1)``
   * - ``6``
     - ``2nd LV GVM (LV1 / TGVM)``


Socket APIs
-----------

habmm_socket_open
^^^^^^^^^^^^^^^^^

.. c:function:: int32_t habmm_socket_open(int32_t *handle, uint32_t mm_ip_id, uint32_t timeout, uint32_t flags)

   Open a HAB virtual channel for ``mm_ip_id`` and return a channel handle.

   :param handle: Output virtual-channel handle.
   :param mm_ip_id: HAB MMID used to select the target HAB client/channel.
      MMIDs use ``HAB_MMID_CREATE(major, minor)`` encoding
      (``(major & 0xFFFF) | ((minor & 0xFF) << 16)``), where major MMID maps
      to a physical HAB channel and minor MMID is sub-identification within the
      major domain. Open succeeds only when both major and minor MMID match on
      both VM endpoints. See :ref:`hab-mmid-mapping-table`.
   :param timeout: Open timeout in ms.
   :param flags: Reserved for future usage.

   .. note::

      **Blocking behavior**

      * This API is synchronous and can block in the channel-open handshake.

   **Flags**

   .. list-table::
      :header-rows: 1

      * - Flag
        - Value
        - Meaning
      * - ``HABMM_SOCKET_OPEN_FLAGS_SINGLE_BE_SINGLE_FE``
        - ``0x00000000``
        - Single FE-to-BE point-to-point mode (default).

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - Channel opened successfully.
        - Use ``*handle`` for further HAB send/recv/export/import/query.
      * - ``-EINVAL``
        - Invalid input (for example ``handle == NULL`` or non-existed MMID).
        - Check HAB system logs and examine arguments.
      * - ``-ENODEV``
        - No physical channel found.
        - Examine MMID mapping and HAB readiness.
      * - ``-ETIMEDOUT``
        - Open handshake timed out.
        - Confirm remote endpoint is alive and call habmm_socket_open with exact **SAME** mmid as well.
      * - ``-EPROTO``
        - FE/BE major HAB API version mismatch during open handshake.
        - Stop retrying and align FE/BE HAB versions.
      * - ``-EINTR``
        - Blocking ``habmm_socket_open()`` was interrupted because the caller thread/process received a signal.
        - Usually retry open on the same MMID if caller still expects the channel; otherwise follow caller shutdown policy.
      * - ``-ENXIO``
        - The current MMID (Major+Minor) is forbidden/unavailable for this context.
        - Stop retry; use another valid MMID.
      * - ``-ENOMEM``
        - Memory allocation failed while creating/opening channel resources.
        - Retry after resource pressure is reduced.

habmm_socket_close
^^^^^^^^^^^^^^^^^^

.. c:function:: int32_t habmm_socket_close(int32_t handle)

   Close a previously opened HAB virtual channel.

   :param handle: Channel handle returned by :c:func:`habmm_socket_open`.

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - Channel closed successfully.
        - N/A.
      * - ``-ENODEV``
        - Handle is unknown/already gone.
        - Examine the correctness of input handle.
      * - ``-EINVAL``
        - Invalid context.
        - Validate close path state and avoid duplicate/invalid closes.
      * - ``-ENOMEM``
        - Temporary allocation failure in close cleanup path.
        - Retry close during low-memory windows.

habmm_socket_send
^^^^^^^^^^^^^^^^^

.. c:function:: int32_t habmm_socket_send(int32_t handle, void *src_buff, uint32_t size_bytes, uint32_t flags)

   Send one message on a channel.

   :param handle: Channel handle.
   :param src_buff: Message buffer.
   :param size_bytes: Message size.
   :param flags: Send flags such as ``HABMM_SOCKET_SEND_FLAGS_*``.

   **Flags**

   .. list-table::
      :header-rows: 1

      * - Flag
        - Value
        - Meaning
      * - ``0`` (no flags)
        - ``0x00000000``
        - Default send behavior.
      * - ``HABMM_SOCKET_SEND_FLAGS_NON_BLOCKING``
        - ``0x00000001``
        - Return ``-EAGAIN`` if send cannot complete immediately.
      * - ``HABMM_SOCKET_SEND_FLAGS_XING_VM_STAT``
        - ``0x00000002``
        - Enable cross-VM timestamp/stat collection payload handling.
      * - ``HABMM_SOCKET_XVM_SCHE_TEST``
        - ``0x00000004``
        - Start cross-VM scheduling-latency measurement flow.
      * - ``HABMM_SOCKET_XVM_SCHE_TEST_ACK``
        - ``0x00000008``
        - Acknowledge scheduling-latency test message.
      * - ``HABMM_SOCKET_XVM_SCHE_RESULT_REQ``
        - ``0x00000010``
        - Request aggregated scheduling-latency result values.
      * - ``HABMM_SOCKET_XVM_SCHE_RESULT_RSP``
        - ``0x00000020``
        - Respond with scheduling-latency result values.

   .. note::

      In real use cases, ``HABMM_SOCKET_SEND_FLAGS_NON_BLOCKING`` is the only
      commonly used public flag. The cross-VM stats/scheduling flags are used by
      HAB internal test/measurement flows and are not intended for normal client
      data-path usage.

   **Blocking behavior**

   ``habmm_socket_send()`` may block in default mode on the following platforms:

   * In QNX, the underlying transport buffer is full (for example, peer is not receiving).
   * In Linux/Android GVM, the virtio out buffer is not returned from PVM vhost side
     (for example, sending too fast).

   This behavior does **not** apply to Linux PVM HAB driver.

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - Message accepted by HAB transport.
        - Continue normal flow.
      * - ``-EINVAL``
        - Invalid size/flag combination or payload constraints violated.
        - Fix payload size/alignment/flag usage; do not blind-retry.
      * - ``-ENODEV``
        - Channel handle invalid or remote end closed.
        - Examine the correctness of the channel handle; channel is already closed and cannot resume, open new channel or start cleanup.
      * - ``-EAGAIN``
        - Non-blocking send cannot complete immediately.
        - Retry later, avoid tight spinning; check the status of the other end.

habmm_socket_recv
^^^^^^^^^^^^^^^^^

.. c:function:: int32_t habmm_socket_recv(int32_t handle, void *dst_buff, uint32_t *size_bytes, uint32_t timeout, uint32_t flags)

   Receive one message from a channel.

   :param handle: Channel handle.
   :param dst_buff: Receive buffer.
   :param size_bytes: In: buffer size, Out: actual message size.
   :param timeout: Timeout in ms (interpreted with ``HABMM_SOCKET_RECV_FLAGS_TIMEOUT``).
   :param flags: Receive flags such as ``HABMM_SOCKET_RECV_FLAGS_*``.

   **Flags**

   .. list-table::
      :header-rows: 1

      * - Flag
        - Value
        - Meaning
      * - ``0`` (no flags)
        - ``0x00000000``
        - Default blocking receive behavior without timeout.
      * - ``HABMM_SOCKET_RECV_FLAGS_NON_BLOCKING``
        - ``0x00000001``
        - Non-blocking receive; return ``-EAGAIN`` if no message is available.
      * - ``HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE``
        - ``0x00000002``
        - Use uninterruptible wait in blocking mode.
      * - ``HABMM_SOCKET_RECV_FLAGS_TIMEOUT``
        - ``0x00000004``
        - Enable timeout-based blocking receive using ``timeout`` (ms).

   .. note::

      **Blocking behavior**

      * By default, the caller will blocking wait on the channel if there is no message available.
      * ``HABMM_SOCKET_RECV_FLAGS_NON_BLOCKING``: never blocks; returns ``-EAGAIN`` if no message is available.
      * Blocking mode without ``HABMM_SOCKET_RECV_FLAGS_TIMEOUT``: waits indefinitely for data or channel close.
      * Blocking mode with ``HABMM_SOCKET_RECV_FLAGS_TIMEOUT``: waits up to ``timeout`` ms and returns ``-ETIMEDOUT`` if no data arrives.
      * Default blocking wait is interruptible and may return ``-EINTR``.
      * If ``HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE`` is set, wait is non-interruptible and typically ends only on data/timeout/channel-close.

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - Message received; ``*size_bytes`` is payload length.
        - Process payload.
      * - ``-EINVAL``
        - Invalid arguments (for example ``dst_buff == NULL`` or ``size_bytes == NULL``).
        - Fix caller input.
      * - ``-EOVERFLOW``
        - Receive buffer too small; ``*size_bytes`` contains required size.
        - Reallocate/resize buffer to ``*size_bytes`` and retry receive.
      * - ``-EAGAIN``
        - No message currently available (most commonly non-blocking receive path).
        - Confirm whether a message was sent from the other end; retry ``habmm_socket_recv()`` on the same HAB socket.
      * - ``-ETIMEDOUT``
        - Timeout expired before data arrived.
        - Confirm whether a message was sent from the other end; retry if operation is still expected.
      * - ``-EINTR``
        - Blocking ``habmm_socket_recv()`` was interrupted because the caller thread/process received a signal.
        - 1. if in user space, retry ``habmm_socket_recv()`` on the same HAB socket if receive flow should continue 2. if in kernel, return to user-space to let signal get handled.
      * - ``-ENODEV``
        - Invalid handle; The channel is disconnected (remote endpoint closed).
        - Close this channel instead of retrying ``habmm_socket_recv()``; The indication of GVM shutdown, start cleanup.

.. c:struct:: hab_socket_info

   Output structure populated by :c:func:`habmm_socket_query`. Provides local and
   remote HAB OSID and VM name for an open channel.

   .. c:member:: int32_t vmid_remote

      Remote HAB OSID. See :ref:`hab-osid-mapping-table`.

   .. c:member:: int32_t vmid_local

      Local HAB OSID. See :ref:`hab-osid-mapping-table`.

   .. c:member:: char vmname_remote[12]

      Remote VM name string from the hypervisor framework (up to 11 characters,
      null-terminated). May be empty if the hypervisor does not provide VM names.

   .. c:member:: char vmname_local[12]

      Local VM name string from the hypervisor framework (up to 11 characters,
      null-terminated). May be empty if the hypervisor does not provide VM names.

habmm_socket_query
^^^^^^^^^^^^^^^^^^

.. c:function:: int32_t habmm_socket_query(int32_t handle, struct hab_socket_info *info, uint32_t flags)

   Query local/remote VM information (local and remote HAB OSIDs, local and remote VM names) for an open channel. See :ref:`hab-osid-mapping-table`.

   :param handle: Channel handle.
   :param info: Output socket info (contains local/remote HAB OSIDs). See :ref:`hab-osid-mapping-table`.
   :param flags: Reserved.

   **Flags**

   .. list-table::
      :header-rows: 1

      * - Flag
        - Value
        - Meaning
      * - ``0`` (reserved)
        - ``0x00000000``
        - No optional behavior defined.

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - ``info`` populated.
        - Use HAB OSIDs/names for routing/logging. See :ref:`hab-osid-mapping-table`.
      * - ``-EINVAL``
        - Invalid ``info`` pointer or invalid handle.
        - Fix arguments or verify handle ownership.
      * - ``-ENODEV``
        - Channel/remote side no longer valid.
        - Refresh channel state (re-open if required).


Memory Share APIs
-----------------

habmm_export
^^^^^^^^^^^^

.. c:function:: int32_t habmm_export(int32_t handle, void *buff_to_share, uint32_t size_bytes, uint32_t *export_id, uint32_t flags)

   Export a memory region so a remote HAB endpoint can import it.

   :param handle: Channel handle.
   :param buff_to_share: Buffer base address (or FD-based export via flags).
   :param size_bytes: Export size in bytes.
   :param export_id: Output export identifier.
   :param flags: Export flags such as ``HABMM_EXPIMP_FLAGS_*``.

   **Flags**

   .. list-table::
      :header-rows: 1

      * - Flag
        - Value
        - Meaning
      * - ``0`` (no flags)
        - ``0x00000000``
        - Indicating the ``buff_to_share`` is a non dma-buf backed virtual address from user-space.
      * - ``HABMM_EXP_MEM_TYPE_DMA``
        - ``0x00000001``
        - Indicating the ``buff_to_share`` is a dma-buf backed virtual address from user-space (Linux-specific).
      * - ``HABMM_EXP_MEM_TYPE_LOOPBACK``
        - ``0x00000002``
        - Export loopback memory (remote exported, local imported then local re-exported it back to the same GVM); must be combined with ``HABMM_EXPIMP_FLAGS_FD`` or ``HABMM_EXPIMP_FLAGS_DMABUF``.
      * - ``HABMM_EXPIMP_FLAGS_FD``
        - ``0x00010000``
        - Indicating the ``buff_to_share`` is a file descriptor (Linux-specific) or a memory object handle (QNX-specific).
      * - ``HABMM_EXPIMP_FLAGS_DMABUF``
        - ``0x00020000``
        - Indicating the ``buff_to_share`` is a dma-buf pointer from kernel. (Linux-specific).

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - Export successfully; ``*export_id`` is allocated.
        - Send ``export_id`` and ``size_bytes`` to remote peer over HAB channel.
      * - ``-EINVAL``
        - Invalid arguments (for example null ``export_id``) or invalid export size/alignment.
        - Ensure page-aligned, non-zero size and valid pointers/flags.
      * - ``-ENODEV``
        - Invalid channel or unavailable remote path.
        - Revalidate channel and retry after re-open.
      * - ``-ENOMEM``
        - Resource allocation failure during export/map setup.
        - Retry later and reduce pressure (fewer/lower-size exports).
      * - ``-EFAULT``
        - Underlying memory mapping/validation failed.
        - Validate buffer origin and access permissions before retry.
      * - ``-EBADF``
        - dmabuf attach or map attachment failure.
        - Check HAB error logs in dmesg. Validate the correctness of the input dmabuf FD or pointer.
      * - ``+EINVAL`` ``+ENODEV`` ``+ENOMEM`` ``+ENOENT``
        - QNX memory subsystem error during export setup.
        - Check QNX system logs.

habmm_unexport
^^^^^^^^^^^^^^

.. c:function:: int32_t habmm_unexport(int32_t handle, uint32_t export_id, uint32_t flags)

   Unexport a previously exported memory object.

   :param handle: Channel handle.
   :param export_id: Export identifier returned by :c:func:`habmm_export`.
   :param flags: Reserved.

   **Flags**

   .. list-table::
      :header-rows: 1

      * - Flag
        - Value
        - Meaning
      * - ``0`` (reserved)
        - ``0x00000000``
        - No optional behavior defined.

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - Export released.
        - Remove local tracking for ``export_id``.
      * - ``-EINVAL``
        - Invalid ``export_id`` or incorrect export ownership/state.
        - Verify ID/owner and deduplicate unexport calls.
      * - ``-EBUSY``
        - Remote still has the export imported.
        - Delay unexport until peer unimports; retry after handshake/ack.
      * - ``-ENODEV``
        - Channel no longer valid.
        - Treat as disconnected; recover channel first.

habmm_import
^^^^^^^^^^^^

.. c:function:: int32_t habmm_import(int32_t handle, void **buff_shared, uint32_t size_bytes, uint32_t export_id, uint32_t flags)

   Import memory previously exported by the remote endpoint.

   :param handle: Channel handle.
   :param buff_shared: Output imported object (dma-buf kernel pointer/dma-buf fd/memory object handle/virtual address as configured).
   :param size_bytes: Expected import size (must match exported size).
   :param export_id: Remote export identifier.
   :param flags: Alternative import behavior.

   **Flags**

   .. list-table::
      :header-rows: 1

      * - Flag
        - Value
        - Meaning
      * - ``0`` (no flags)
        - ``0x00000000``
        - Indicating the output of ``buff_shared`` is expected to be a virtual address.
      * - ``HABMM_IMPORT_FLAGS_CACHED``
        - ``0x00000001``
        - Request cacheable mapping when mmap the dma-buf fd (Linux-specific) or enable cacheable attribute for the imported virtual address.
      * - ``HABMM_EXPIMP_FLAGS_FD``
        - ``0x00010000``
        - Indicating the output of ``buff_shared`` is expected to be a dma-buf FD (Linux-specific) or a memory object handle (QNX-specific).
      * - ``HABMM_EXPIMP_FLAGS_DMABUF``
        - ``0x00020000``
        - Indicating the output of ``buff_shared`` is expected to be a dma-buf kernel pointer (Linux-specific).

   .. note::

      **Blocking behavior**

      * Import is synchronous.
      * It blocks waiting for remote import-ack (handled by HAB driver internally).
      * Import-ack wait uses an internal handshake, interruption is normalized to ``-EAGAIN`` by lower layer.
      * Import can also fail immediately (no wait) on invalid parameters, missing export descriptor, or channel state errors.

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - Import successful; ``*buff_shared`` is valid.
        - Use mapped object and pair with :c:func:`habmm_unimport`.
      * - ``-EINVAL``
        - Invalid pointer/size/alignment/state or size mismatch with exporter.
        - Validate page alignment, size contract, and single-import semantics.
      * - ``-ENODEV``
        - Channel/export descriptor unavailable.
        - Re-sync with peer and channel state before retry.
      * - ``-ENOMEM``
        - Insufficient system memory to create import object.
        - Retry after system memory pressure is decreased.
      * - ``-EAGAIN``
        - Import handshake was interrupted.
        - Re-check channel liveness and export ID ownership, then retry.
      * - ``+ENODEV`` ``+EINVAL`` ``+EACCESS`` ``+EFAULT`` ``+ENOMEM`` ``+EAGAIN``
        - QNX memory subsystem error during import setup.
        - Check QNX system logs.

habmm_unimport
^^^^^^^^^^^^^^

.. c:function:: int32_t habmm_unimport(int32_t handle, uint32_t export_id, void *buff_shared, uint32_t flags)

   Unimport memory resources from a prior import.

   :param handle: Channel handle.
   :param export_id: Export identifier used for import.
   :param buff_shared: Imported object returned by :c:func:`habmm_import`.
   :param flags: Unimport flags (for example ``HABMM_UNIMP_FLAGS_FD_ALREADY_CLOSED``).

   ``habmm_unimport()`` uses strong unimport semantics for imported dma-buf:
   unimport succeeds only when local references are at expected idle count.
   If the imported object is still in use, unimport returns busy instead of
   forcing teardown.

   **Flags**

   .. list-table::
      :header-rows: 1

      * - Flag
        - Value
        - Meaning
        - When to use
      * - ``0`` (no flags)
        - ``0x00000000``
        - Default unimport behavior.
        - In most cases.
      * - ``HABMM_UNIMP_FLAGS_FD_ALREADY_CLOSED``
        - ``0x00040000``
        - Indicates the importer-side dma-buf FD (returned by user-space import path) was already closed before calling unimport.
        - User-space client sets this when it already closed imported FD and then calls unimport.

   .. note::

      Internally, unimport checks dma-buf idle file-refcount before completing.
      For user-space callers:
      ``flags`` with ``HABMM_UNIMP_FLAGS_FD_ALREADY_CLOSED`` expects idle count ``1``;
      otherwise expected idle count is ``2``.
      For kernel callers (``khab`` API path), expected idle count is always ``1`` and
      ``flags`` do not change this behavior.

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - Unimport successful.
        - N/A.
      * - ``-EINVAL``
        - ``export_id`` is unavailable (not imported, already unimported, or in-flight race).
        - Verify ``export_id``/``handle`` pairing and import-unimport ordering; also check for concurrent duplicate unimport calls on the same ``export_id``.
      * - ``-ENODEV``
        - ``handle`` is unavailable.
        - Verify the correctness of ``handle``.
      * - ``-EBUSY``
        - Imported dma-buf is still in use (file refcount higher than expected idle count).
        - Drop local references/mappings and **SMMU mappings** if present, then retry ``habmm_unimport()`` with the same ``export_id``.
      * - ``-ENOENT``
        - Imported dma-buf refcount is lower than expected (unexpected lifetime state, for example premature close/release).
        - #. If called in user space, check whether FD was already closed without passing ``HABMM_UNIMP_FLAGS_FD_ALREADY_CLOSED``.
          #. If called in kernel, check redundant ``dma_buf_put()`` calls.
      * - ``+EBUSY``
        - QNX memory subsystem error; imported object still in use (QNX-specific).
        - Drop local references/mappings and **SMMU mappings** if present, then retry ``habmm_unimport()`` with the same ``export_id``.
      * - ``+EINVAL`` ``+ENODEV`` ``+ENOENT``
        - QNX memory subsystem error during unimport (QNX-specific).
        - Check QNX system logs.


VIRQ APIs
---------

VIRQ numbers are defined in:

* ``include/uapi/linux/habmmid.h`` in Linux
* ``habmmid.h`` in QNX

.. list-table::
   :header-rows: 1

   * - VIRQ macro
     - Value
     - Mapped domain
   * - ``VIRQ_AUD``
     - ``1001``
     - ``AUD`` domain
   * - ``VIRQ_CAM``
     - ``2001``
     - ``CAM`` domain
   * - ``VIRQ_DISP1``
     - ``3001``
     - ``DISP`` domain (path 1)
   * - ``VIRQ_DISP2``
     - ``3002``
     - ``DISP`` domain (path 2)
   * - ``VIRQ_DPRX1``
     - ``3003``
     - ``DPRX`` domain (path 1)
   * - ``VIRQ_DPRX2``
     - ``3004``
     - ``DPRX`` domain (path 2)
   * - ``VIRQ_GFX``
     - ``4001``
     - ``GFX`` domain
   * - ``VIRQ_VID``
     - ``5001``
     - ``VID`` domain
   * - ``VIRQ_MISC``
     - ``6001``
     - ``MISC`` domain

.. c:type:: virq_rx_cb_t

   ``typedef int32_t (*virq_rx_cb_t)(int32_t irq, void *priv_data, uint32_t flags)``

   RX callback type for :c:func:`habmm_virq_register`. Invoked by the HAB driver
   when a virtual IRQ fires on the registered endpoint.

   :param irq: VIRQ number that fired (matches ``virq_num`` used during registration).
   :param priv_data: Private data pointer passed as ``rx_priv`` at registration time.
   :param flags: Reserved; currently unused.
   :returns: ``0`` on success; negative errno on error (return value is currently
      ignored by the HAB driver).

habmm_virq_register
^^^^^^^^^^^^^^^^^^^

.. c:function:: int32_t habmm_virq_register(int32_t *handle, uint32_t vmid, uint32_t virq_num, virq_rx_cb_t rx_cb, void *rx_priv, uint32_t flags)

   Register a virtual IRQ endpoint for TX or RX.

   :param handle: Output virq handle.
   :param vmid: Remote VM ID. See :ref:`hab-osid-mapping-table`.
   :param virq_num: Virtual IRQ number for the target domain, typically one of
      the ``VIRQ_*`` values listed in the VIRQ mapping table above.
   :param rx_cb: RX callback (for RX registration mode).
   :param rx_priv: Callback private data or userspace eventfd context.
   :param flags: ``HABMM_VIRQ_FLAGS_TX`` or ``HABMM_VIRQ_FLAGS_RX``.

   **Flags**

   .. list-table::
      :header-rows: 1

      * - Flag
        - Value
        - Meaning
      * - ``HABMM_VIRQ_FLAGS_TX``
        - ``0x00000001``
        - Register TX virtual IRQ endpoint.
      * - ``HABMM_VIRQ_FLAGS_RX``
        - ``0x00000002``
        - Register RX virtual IRQ endpoint.

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - Registration successful; ``*handle`` is valid.
        - Store handle and use matching unregister path.
      * - ``-EINVAL``
        - Invalid HAB OSID/flags/arguments.
        - Validate HAB OSID range and exactly one RX/TX mode.
      * - ``-ENODEV``
        - Requested virq label/path unavailable.
        - Verify platform DT/hypervisor virq provisioning.
      * - ``-EFAULT``
        - QNX subsystem error (QNX-specific).
        - Retry after checking QNX subsystem status.
      * - ``-EOPNOTSUPP``
        - HAB Virq is not supported on this target (Linux-specific).
        - Update libuhab if needed.

habmm_send_virq
^^^^^^^^^^^^^^^

.. c:function:: int32_t habmm_send_virq(int32_t handle, uint32_t flags)

   Send a virtual IRQ to the remote endpoint using a registered TX virq handle.
   This API is implemented by the lower-layer ``hab_virq_send()`` path.

   :param handle: VIRQ handle from :c:func:`habmm_virq_register`.
   :param flags: Reserved (future extension).

   **Flags**

   .. list-table::
      :header-rows: 1

      * - Flag
        - Value
        - Meaning
      * - ``0`` (reserved)
        - ``0x00000000``
        - No optional behavior defined.

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - VIRQ sent successfully.
        - Continue normal flow.
      * - ``-EAGAIN`` (Linux-specific)
        - Gunyah doorbell driver returned ``-EAGAIN``.
        - Unregister and re-register the VIRQ endpoint; if it persists, verify Gunyah doorbell provisioning.
      * - ``-ENODEV``
        - VIRQ handle is invalid/unavailable.
        - Unregister and re-register the VIRQ endpoint to obtain a valid handle.
      * - ``-EOPNOTSUPP``
        - HAB Virq is not supported on this target (Linux-specific).
        - Update libuhab if needed.

habmm_virq_unregister
^^^^^^^^^^^^^^^^^^^^^

.. c:function:: int32_t habmm_virq_unregister(int32_t handle, uint32_t flags)

   Unregister a previously registered virtual IRQ endpoint.

   :param handle: VIRQ handle from :c:func:`habmm_virq_register`.
   :param flags: RX/TX mode for the registration being removed.

   **Flags**

   .. list-table::
      :header-rows: 1

      * - Flag
        - Value
        - Meaning
      * - ``HABMM_VIRQ_FLAGS_TX``
        - ``0x00000001``
        - Unregister TX virtual IRQ endpoint.
      * - ``HABMM_VIRQ_FLAGS_RX``
        - ``0x00000002``
        - Unregister RX virtual IRQ endpoint.

   **Return values and client actions**

   .. list-table::
      :header-rows: 1

      * - Return value
        - Meaning
        - Suggested HAB client action
      * - ``0``
        - Unregister successful.
        - Clear local handle state.
      * - ``-EINVAL``
        - Invalid handle/flags.
        - Verify handle ownership and RX/TX flag pairing.
      * - ``-EOPNOTSUPP``
        - Does not support virq operations.
        - Update libuhab if needed.
