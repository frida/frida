use core::cell::UnsafeCell;
use core::ffi::c_void;
use core::mem::size_of;
use core::ptr::{read_volatile, write_volatile};
use core::sync::atomic::{AtomicUsize, Ordering};

use crate::gum::gum_barebone_query_page_size;
use crate::kernel;

const MMIO_SIZE: u64 = 0x200;

static PAGE_SIZE: AtomicUsize = AtomicUsize::new(0);

const QSZ: u16 = 64;

const MAGIC: usize = 0x000;
const VERSION: usize = 0x004;
const DEVICE: usize = 0x008;
const STATUS: usize = 0x070;

const DEVFEAT: usize = 0x010;
const DEVFEAT_SEL: usize = 0x014;
const DRVFEAT: usize = 0x020;
const DRVFEAT_SEL: usize = 0x024;

const QSEL: usize = 0x030;
const QNUM_MAX: usize = 0x034;
const QNUM: usize = 0x038;
const QREADY: usize = 0x044;

const QDESC_LO: usize = 0x080;
const QDESC_HI: usize = 0x084;
const QAVAIL_LO: usize = 0x090;
const QAVAIL_HI: usize = 0x094;
const QUSED_LO: usize = 0x0a0;
const QUSED_HI: usize = 0x0a4;

const QNOTIFY: usize = 0x050;
const ISR: usize = 0x060;
const ISR_ACK: usize = 0x064;

const ST_ACK: u32 = 1;
const ST_DRV: u32 = 2;
const ST_DRV_OK: u32 = 4;
const ST_FEAT_OK: u32 = 8;
const ST_FAILED: u32 = 0x80;

const DEV_ID_CONSOLE: u32 = 3;
const F_VERSION_1: u64 = 1u64 << 32;
const F_MULTIPORT: u64 = 1u64 << 1;

const INT_VRING: u32 = 1;

const PCI_CONFIG_ADDRESS: u16 = 0xcf8;
const PCI_CONFIG_DATA: u16 = 0xcfc;

const PCI_VENDOR_VIRTIO: u16 = 0x1af4;
const PCI_DEVICE_CONSOLE_LEGACY: u16 = 0x1003;
const PCI_DEVICE_CONSOLE_MODERN: u16 = 0x1043;

const PCI_COMMAND: u8 = 0x04;
const PCI_CAP_LIST_POINTER: u8 = 0x34;
const PCI_INTERRUPT_LINE: u8 = 0x3c;
const PCI_BASE_ADDRESS_0: u8 = 0x10;
const PCI_INTERRUPT_PIN: u8 = 0x3d;
const PCI_COMMAND_INTX_DISABLE: u32 = 0x400;
const ISA_BRIDGE_DEVFN: u8 = 0x08;
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
const ECAM_DEVFN_SHIFT: usize = 12;
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
const ECAM_BUS_SIZE: u64 = 256 * 4096;
const PIRQ_ROUTE: u8 = 0x60;
const PIRQ_DISABLED: u32 = 1 << 7;

const PCI_COMMAND_MEMORY: u32 = 1 << 1;
const PCI_COMMAND_BUS_MASTER: u32 = 1 << 2;

const PCI_CAP_ID_VENDOR: u8 = 0x09;
const PCI_CAP_ID_MSI: u8 = 0x05;
const PCI_CAP_ID_MSIX: u8 = 0x11;
const PCI_MSI_ENABLE: u32 = 1 << 16;
const PCI_MSIX_ENABLE: u32 = 1 << 31;

const VIRTIO_CAP_CFG_TYPE: u8 = 3;
const VIRTIO_CAP_BAR: u8 = 4;
const VIRTIO_CAP_OFFSET: u8 = 8;
const VIRTIO_CAP_LENGTH: u8 = 12;
const VIRTIO_CAP_NOTIFY_MULTIPLIER: u8 = 16;

const CFG_TYPE_COMMON: u8 = 1;
const CFG_TYPE_NOTIFY: u8 = 2;
const CFG_TYPE_ISR: u8 = 3;

const COMMON_DEVFEAT_SEL: usize = 0x00;
const COMMON_DEVFEAT: usize = 0x04;
const COMMON_DRVFEAT_SEL: usize = 0x08;
const COMMON_DRVFEAT: usize = 0x0c;
const COMMON_STATUS: usize = 0x14;
const RESET_ATTEMPTS: u32 = 1_000_000;
const COMMON_QSEL: usize = 0x16;
const COMMON_QNUM: usize = 0x18;
const COMMON_QREADY: usize = 0x1c;
const COMMON_QNOTIFY_OFF: usize = 0x1e;
const COMMON_QDESC: usize = 0x20;
const COMMON_QAVAIL: usize = 0x28;
const COMMON_QUSED: usize = 0x30;

const Q_RX0: u16 = 0;
const Q_TX0: u16 = 1;
const Q_CTRL_RX: u16 = 2;
const Q_CTRL_TX: u16 = 3;

#[repr(C)]
#[derive(Copy, Clone)]
struct VConsCtrl {
    id: u32,
    event: u16,
    value: u16,
}
const EV_DEVICE_READY: u16 = 0;
const EV_DEVICE_ADD: u16 = 1;
const EV_PORT_READY: u16 = 3;
const EV_PORT_OPEN: u16 = 6;
const EV_CONSOLE_PORT: u16 = 4;

#[repr(C)]
struct Desc {
    addr: u64,
    len: u32,
    flags: u16,
    next: u16,
}
const D_NEXT: u16 = 1;
const D_WRITE: u16 = 2;

#[repr(C)]
#[derive(Copy, Clone)]
struct Avail {
    flags: u16,
    idx: u16,
}

#[repr(C)]
#[derive(Copy, Clone)]
struct UsedElem {
    id: u32,
    len: u32,
}
#[repr(C)]
#[derive(Copy, Clone)]
struct Used {
    flags: u16,
    idx: u16,
}

#[derive(Copy, Clone)]
struct DmaPage {
    va: *mut u8,
    pa: u64,
}

fn dma_page_alloc() -> DmaPage {
    let len = PAGE_SIZE.load(Ordering::Relaxed);
    let va = kernel::alloc_dma(len);
    let pa = kernel::virt_to_phys(va as u64);
    DmaPage { va, pa }
}

fn dma_page_free(p: DmaPage) {
    let len = PAGE_SIZE.load(Ordering::Relaxed);
    kernel::free_dma(p.va, len);
}

struct Vq {
    sel: u16,
    notify_off: u16,
    size: u16,
    desc_va: *mut u8,
    avail_va: *mut u8,
    used_va: *mut u8,
    avail_idx: u16,
    used_idx: u16,
    free_head: u16,
    free_cnt: u16,
}
impl Vq {
    fn new(regs: &Regs, sel: u16, size: u16) -> Self {
        let size = size.min(regs.queue_max(sel));

        let d = dma_page_alloc();
        let a = dma_page_alloc();
        let u = dma_page_alloc();

        let ps = PAGE_SIZE.load(core::sync::atomic::Ordering::Relaxed);
        unsafe {
            core::ptr::write_bytes(d.va, 0, ps);
            core::ptr::write_bytes(a.va, 0, ps);
            core::ptr::write_bytes(u.va, 0, ps);
        }

        let dp = d.va as *mut Desc;
        for i in 0..size {
            unsafe {
                (*dp.add(i as usize)).flags = 0;
                (*dp.add(i as usize)).next = if i + 1 < size { i + 1 } else { 0xFFFF };
            }
        }

        let notify_off = regs.queue_prepare(sel, size, d.pa, a.pa, u.pa);

        Self {
            sel,
            notify_off,
            size,
            desc_va: d.va,
            avail_va: a.va,
            used_va: u.va,
            avail_idx: 0,
            used_idx: 0,
            free_head: 0,
            free_cnt: size,
        }
    }

    fn alloc(&mut self) -> u16 {
        debug_assert!(self.free_cnt > 0);
        let h = self.free_head;
        let dp = self.desc_va as *mut Desc;
        unsafe {
            self.free_head = (*dp.add(h as usize)).next;
        }
        self.free_cnt -= 1;
        h
    }

    fn free_chain(&mut self, mut idx: u16) {
        let dp = self.desc_va as *mut Desc;
        let head = idx;
        loop {
            self.free_cnt += 1;
            let (flags, next) = unsafe {
                let p = dp.add(idx as usize);
                ((*p).flags, (*p).next)
            };
            if (flags & D_NEXT) == 0 {
                break;
            }
            idx = next;
        }
        unsafe {
            (*dp.add(idx as usize)).next = self.free_head;
        }
        self.free_head = head;
    }

    fn push_avail(&mut self, head: u16) {
        let ap = self.avail_va as *mut Avail;
        let ring = unsafe { (ap as *mut u8).add(size_of::<Avail>()) as *mut u16 };
        let slot = (self.avail_idx % self.size) as usize;
        unsafe {
            ring.add(slot).write_volatile(head);
        }
        wmb();
        self.avail_idx = self.avail_idx.wrapping_add(1);
        unsafe {
            (&raw mut (*ap).idx).write_volatile(self.avail_idx);
        }
    }

    fn pop_used(&mut self) -> Option<UsedElem> {
        let up = self.used_va as *mut Used;
        if unsafe { (&raw const (*up).idx).read_volatile() } == self.used_idx {
            return None;
        }
        rmb();
        let ring = unsafe { (up as *mut u8).add(size_of::<Used>()) as *mut UsedElem };
        let elem = unsafe { ring.add((self.used_idx % self.size) as usize).read_volatile() };
        self.used_idx = self.used_idx.wrapping_add(1);
        Some(elem)
    }
}

struct Inner {
    regs: Regs,

    ctrl_rx: Vq,
    ctrl_tx: Vq,

    port_id: Option<u32>,
    rx: Option<Vq>,
    tx: Option<Vq>,

    rx_need: usize,
    rx_have: usize,
    rx_lenbuf: [u8; 4],
    rx_lenhave: usize,
    rx_buf: Option<&'static mut [u8]>,

    tx_head: *mut TxNode,
    tx_tail: *mut TxNode,

    ctrl_rx_pages: [Option<DmaPage>; QSZ as usize],
    data_rx_pages: [Option<DmaPage>; QSZ as usize],
    tx_pages: [Option<DmaPage>; QSZ as usize],

    wake_token: *const u8,

    on_rx: Option<fn(&[u8])>,
}

#[repr(C)]
struct TxNode {
    next: *mut TxNode,
    frame: &'static [u8],
    sent: usize,
}

pub struct Hostlink {
    state: alloc::boxed::Box<UnsafeCell<Inner>>,
}

unsafe impl Send for Hostlink {}

impl Hostlink {
    pub fn shutdown(&self) {
        let inner = unsafe { &mut *self.state.get() };
        inner.regs.set_status(0);
    }

    pub fn init(mmio_base: u64, irq_line: u32, on_rx: Option<fn(&[u8])>, wake_token: *const u8) -> Result<Self, ()> {
        let mmio = kernel::map_io(mmio_base, MMIO_SIZE) as *mut u8;
        if mmio.is_null() {
            return Err(());
        }
        let regs = Regs::Mmio(mmio);

        regs.reset();

        let magic_ok = r32(mmio, MAGIC) == 0x7472_6976;
        let version_ok = r32(mmio, VERSION) == 2;
        let device_ok = r32(mmio, DEVICE) == DEV_ID_CONSOLE;
        if !(magic_ok && version_ok && device_ok) {
            regs.set_status(ST_FAILED);
            return Err(());
        }

        #[cfg(feature = "linux-injected")]
        let irq_line = kernel::mmio_interrupt(mmio_base).unwrap_or(irq_line);
        Self::start(regs, Some(irq_line), on_rx, wake_token)
    }

    pub fn init_pci(ecam: u64, on_rx: Option<fn(&[u8])>, wake_token: *const u8) -> Result<Self, ()> {
        let Some(device) = PciDevice::find_console(ecam) else {
            return Err(());
        };

        let Some(regs) = device.map_virtio_regs() else {
            return Err(());
        };

        regs.reset();

        Self::start(regs, device.irq_line(), on_rx, wake_token)
    }

    fn start(regs: Regs, irq_line: Option<u32>, on_rx: Option<fn(&[u8])>, wake_token: *const u8) -> Result<Self, ()> {
        let page_size = gum_barebone_query_page_size();
        PAGE_SIZE.store(page_size as usize, Ordering::Relaxed);

        let dev_lo = regs.feat_get(0) as u64;
        let dev_hi = (regs.feat_get(1) as u64) << 32;
        let mut drv: u64 = 0;
        if (dev_hi & F_VERSION_1) != 0 {
            drv |= F_VERSION_1;
        }
        if (dev_lo & F_MULTIPORT) != 0 {
            drv |= F_MULTIPORT;
        }
        regs.feat_set(0, (drv & 0xffff_ffff) as u32);
        regs.feat_set(1, (drv >> 32) as u32);
        regs.set_status(regs.status() | ST_FEAT_OK);
        let feats_ok = (regs.status() & ST_FEAT_OK) != 0;
        if !feats_ok {
            regs.set_status(ST_FAILED);
            return Err(());
        }

        let ctrl_rx = Vq::new(&regs, Q_CTRL_RX, QSZ);
        let ctrl_tx = Vq::new(&regs, Q_CTRL_TX, QSZ);

        unsafe {
            ISR_REGS = Some(regs);
        }
        if let Some(irq_line) = irq_line {
            kernel::install_interrupt_handler(
                irq_line,
                wake_token as *mut c_void,
                isr_wake,
                core::ptr::null_mut(),
            );
        }

        regs.set_status(regs.status() | ST_DRV_OK);

        let inner = Inner {
            regs,
            ctrl_rx,
            ctrl_tx,
            port_id: None,
            rx: None,
            tx: None,
            rx_need: 0,
            rx_have: 0,
            rx_lenbuf: [0; 4],
            rx_lenhave: 0,
            rx_buf: None,
            tx_head: core::ptr::null_mut(),
            tx_tail: core::ptr::null_mut(),
            ctrl_rx_pages: [None; QSZ as usize],
            data_rx_pages: [None; QSZ as usize],
            tx_pages: [None; QSZ as usize],
            wake_token,
            on_rx,
        };

        let hl = Hostlink {
            state: alloc::boxed::Box::new(UnsafeCell::new(inner)),
        };

        hl.ctrl_prime_rx(8);
        hl.ctrl_send(VConsCtrl {
            id: 0,
            event: EV_DEVICE_READY,
            value: 1,
        });

        Ok(hl)
    }

    pub fn send(&self, payload: &[u8]) {
        let s = unsafe { &mut *self.state.get() };

        let total = 4 + payload.len();
        let buf = kernel::alloc(total);
        unsafe {
            let len = payload.len() as u32;
            *buf.add(0) = (len & 0xFF) as u8;
            *buf.add(1) = ((len >> 8) & 0xFF) as u8;
            *buf.add(2) = ((len >> 16) & 0xFF) as u8;
            *buf.add(3) = ((len >> 24) & 0xFF) as u8;
            core::ptr::copy_nonoverlapping(payload.as_ptr(), buf.add(4), payload.len());
        }
        let frame: &'static [u8] = unsafe { core::slice::from_raw_parts(buf, total) };

        let node = kernel::alloc(size_of::<TxNode>()) as *mut TxNode;
        unsafe {
            (*node).next = core::ptr::null_mut();
            (*node).frame = frame;
            (*node).sent = 0;
        }

        if s.tx_tail.is_null() {
            s.tx_head = node;
            s.tx_tail = node;
        } else {
            unsafe {
                (*s.tx_tail).next = node;
            }
            s.tx_tail = node;
        }

        A_TURN_IS_WANTED.store(true, core::sync::atomic::Ordering::Release);
        crate::nudge_the_loop(s.wake_token);
    }

    pub fn process(&self) {
        let s = unsafe { &mut *self.state.get() };

        A_TURN_IS_WANTED.store(false, core::sync::atomic::Ordering::Release);
        s.regs.isr_ack();

        self.ctrl_complete();
        self.ctrl_prime_rx(QSZ as usize);

        self.data_rx_complete();
        self.data_tx_complete();
        self.data_tx_push();
        self.data_rx_refill();
    }

    fn ctrl_prime_rx(&self, count: usize) {
        let s = unsafe { &mut *self.state.get() };
        let mut posted = 0usize;
        while posted < count {
            if s.ctrl_rx.free_cnt == 0 {
                break;
            }
            let pg = dma_page_alloc();
            let h = s.ctrl_rx.alloc();
            let d = s.ctrl_rx.desc_va as *mut Desc;
            unsafe {
                (*d.add(h as usize)).addr = pg.pa;
                (*d.add(h as usize)).len = PAGE_SIZE.load(Ordering::Relaxed) as u32;
                (*d.add(h as usize)).flags = D_WRITE;
                (*d.add(h as usize)).next = 0;
            }
            s.ctrl_rx_pages[h as usize] = Some(pg);
            s.ctrl_rx.push_avail(h);
            posted += 1;
        }
        let (sel, notify_off) = (s.ctrl_rx.sel, s.ctrl_rx.notify_off);
        self.kick(sel, notify_off);
    }

    fn ctrl_send(&self, msg: VConsCtrl) {
        let s = unsafe { &mut *self.state.get() };
        let pg = dma_page_alloc();
        unsafe {
            core::ptr::write(pg.va as *mut VConsCtrl, msg);
        }
        let h = s.ctrl_tx.alloc();
        let d = s.ctrl_tx.desc_va as *mut Desc;
        unsafe {
            (*d.add(h as usize)).addr = pg.pa;
            (*d.add(h as usize)).len = size_of::<VConsCtrl>() as u32;
            (*d.add(h as usize)).flags = 0;
            (*d.add(h as usize)).next = 0;
        }
        s.tx_pages[h as usize] = Some(pg);
        s.ctrl_tx.push_avail(h);
        let (sel, notify_off) = (s.ctrl_tx.sel, s.ctrl_tx.notify_off);
        self.kick(sel, notify_off);
    }

    fn ctrl_complete(&self) {
        let s = unsafe { &mut *self.state.get() };
        while let Some(u) = s.ctrl_rx.pop_used() {
            let h = u.id as u16;
            if let Some(pg) = s.ctrl_rx_pages[h as usize].take() {
                let ev = unsafe { *(pg.va as *const VConsCtrl) };
                dma_page_free(pg);

                match ev.event {
                    EV_DEVICE_ADD | EV_CONSOLE_PORT => {
                        if s.port_id.is_none() {
                            self.setup_data_port(ev.id);
                            self.ctrl_send(VConsCtrl {
                                id: ev.id,
                                event: EV_PORT_READY,
                                value: 1,
                            });
                            self.ctrl_send(VConsCtrl {
                                id: ev.id,
                                event: EV_PORT_OPEN,
                                value: 1,
                            });
                        }
                    }
                    _ => {}
                }

                let pg2 = dma_page_alloc();
                let d = s.ctrl_rx.desc_va as *mut Desc;
                unsafe {
                    (*d.add(h as usize)).addr = pg2.pa;
                    (*d.add(h as usize)).len = PAGE_SIZE.load(Ordering::Relaxed) as u32;
                    (*d.add(h as usize)).flags = D_WRITE;
                    (*d.add(h as usize)).next = 0;
                }
                s.ctrl_rx_pages[h as usize] = Some(pg2);
                s.ctrl_rx.push_avail(h);
            }
        }
        let (sel, notify_off) = (s.ctrl_rx.sel, s.ctrl_rx.notify_off);
        self.kick(sel, notify_off);

        while let Some(u) = s.ctrl_tx.pop_used() {
            let head = u.id as u16;
            if let Some(pg) = s.tx_pages[head as usize].take() {
                dma_page_free(pg);
            }
            s.ctrl_tx.free_chain(head);
        }
    }

    fn setup_data_port(&self, id: u32) {
        let s = unsafe { &mut *self.state.get() };
        if s.port_id.is_some() {
            return;
        }
        let (rx_i, tx_i) = if id == 0 {
            (Q_RX0, Q_TX0)
        } else {
            let base = 4 + ((id as u16 - 1) * 2);
            (base, base + 1)
        };
        let rx = Vq::new(&s.regs, rx_i, QSZ);
        let tx = Vq::new(&s.regs, tx_i, QSZ);
        s.rx = Some(rx);
        s.tx = Some(tx);
        s.port_id = Some(id);
        self.data_rx_refill();
    }

    fn data_rx_refill(&self) {
        let s = unsafe { &mut *self.state.get() };
        let Some(rxq) = s.rx.as_mut() else {
            return;
        };
        while rxq.free_cnt > 0 {
            let h = rxq.alloc();
            let pg = dma_page_alloc();
            let d = rxq.desc_va as *mut Desc;
            unsafe {
                (*d.add(h as usize)).addr = pg.pa;
                (*d.add(h as usize)).len = PAGE_SIZE.load(Ordering::Relaxed) as u32;
                (*d.add(h as usize)).flags = D_WRITE;
                (*d.add(h as usize)).next = 0;
            }
            s.data_rx_pages[h as usize] = Some(pg);
            rxq.push_avail(h);
        }
        let (sel, notify_off) = (rxq.sel, rxq.notify_off);
        self.kick(sel, notify_off);
    }

    fn data_rx_complete(&self) {
        loop {
            let Some((head, length, page)) = self.take_used_rx_page() else {
                break;
            };

            let bytes = unsafe { core::slice::from_raw_parts(page.va, length) };
            self.feed_rx_stream(bytes);
            dma_page_free(page);

            self.hand_rx_page_back(head);
        }

        let s = unsafe { &mut *self.state.get() };
        let Some(rxq) = s.rx.as_mut() else {
            return;
        };
        let (sel, notify_off) = (rxq.sel, rxq.notify_off);
        self.kick(sel, notify_off);
    }

    fn take_used_rx_page(&self) -> Option<(u16, usize, DmaPage)> {
        let s = unsafe { &mut *self.state.get() };
        let rxq = s.rx.as_mut()?;

        let used = rxq.pop_used()?;
        let head = used.id as u16;
        let length = used.len as usize;

        s.data_rx_pages[head as usize].take().map(|page| (head, length, page))
    }

    fn hand_rx_page_back(&self, head: u16) {
        let page = dma_page_alloc();

        let s = unsafe { &mut *self.state.get() };
        let Some(rxq) = s.rx.as_mut() else {
            return;
        };

        let d = rxq.desc_va as *mut Desc;
        unsafe {
            (*d.add(head as usize)).addr = page.pa;
            (*d.add(head as usize)).len = PAGE_SIZE.load(Ordering::Relaxed) as u32;
            (*d.add(head as usize)).flags = D_WRITE;
            (*d.add(head as usize)).next = 0;
        }
        s.data_rx_pages[head as usize] = Some(page);
        rxq.push_avail(head);
    }

    fn feed_rx_stream(&self, mut chunk: &[u8]) {
        while !chunk.is_empty() {
            let (taken, ready) = self.feed_rx_piece(chunk);
            chunk = &chunk[taken..];

            if let Some((deliver, frame)) = ready {
                deliver(frame);
            }

            if taken == 0 {
                return;
            }
        }
    }

    fn feed_rx_piece(&self, chunk: &[u8]) -> (usize, Option<(fn(&[u8]), &'static mut [u8])>) {
        let s = unsafe { &mut *self.state.get() };
        let mut taken = 0;

        if s.rx_lenhave < 4 {
            let need = 4 - s.rx_lenhave;
            let take = core::cmp::min(need, chunk.len());
            s.rx_lenbuf[s.rx_lenhave..s.rx_lenhave + take].copy_from_slice(&chunk[..take]);
            s.rx_lenhave += take;
            taken += take;
            if s.rx_lenhave < 4 {
                return (taken, None);
            }

            let len = (s.rx_lenbuf[0] as usize)
                | ((s.rx_lenbuf[1] as usize) << 8)
                | ((s.rx_lenbuf[2] as usize) << 16)
                | ((s.rx_lenbuf[3] as usize) << 24);

            if s.rx_buf.is_none() && len > 0 {
                let buf = kernel::alloc(len);
                let slice: &'static mut [u8] = unsafe { core::slice::from_raw_parts_mut(buf, len) };
                s.rx_buf = Some(slice);
                s.rx_need = len;
                s.rx_have = 0;
            }
        }

        let rest = &chunk[taken..];
        let need = s.rx_need.saturating_sub(s.rx_have);
        let take = core::cmp::min(need, rest.len());
        if let Some(ref mut buf) = s.rx_buf {
            unsafe {
                core::ptr::copy_nonoverlapping(rest.as_ptr(),
                    buf.as_mut_ptr().wrapping_add(s.rx_have), take);
            }
        }
        s.rx_have += take;
        taken += take;

        if s.rx_have != s.rx_need {
            return (taken, None);
        }

        // Detach the frame and clear the receive state before you dispatch. The callback can call
        // process() again for a synchronous host RPC, thus it must start with a clean state.
        let frame = s.rx_buf.take();
        s.rx_need = 0;
        s.rx_have = 0;
        s.rx_lenhave = 0;

        match (s.on_rx, frame) {
            (Some(cb), Some(frame)) => (taken, Some((cb, frame))),
            _ => (taken, None),
        }
    }

    fn data_tx_complete(&self) {
        let s = unsafe { &mut *self.state.get() };
        let Some(txq) = s.tx.as_mut() else {
            return;
        };
        while let Some(u) = txq.pop_used() {
            let mut i = u.id as u16;
            loop {
                if let Some(pg) = s.tx_pages[i as usize].take() {
                    dma_page_free(pg);
                }
                let d = txq.desc_va as *mut Desc;
                let (f, n) = unsafe {
                    let p = d.add(i as usize);
                    ((*p).flags, (*p).next)
                };
                if (f & D_NEXT) == 0 {
                    break;
                }
                i = n;
            }
            txq.free_chain(u.id as u16);
        }
    }

    fn data_tx_push(&self) {
        let s = unsafe { &mut *self.state.get() };
        let Some(txq) = s.tx.as_mut() else {
            return;
        };
        while !s.tx_head.is_null() {
            if txq.free_cnt == 0 {
                return;
            }

            let node = s.tx_head;
            let frame = unsafe { (*node).frame };
            let mut off = unsafe { (*node).sent };

            let mut head: Option<u16> = None;
            let mut prev = 0u16;

            while off < frame.len() && txq.free_cnt != 0 {
                let page = PAGE_SIZE.load(Ordering::Relaxed);
                let chunk = core::cmp::min(page, frame.len() - off);
                let pg = dma_page_alloc();
                unsafe {
                    core::ptr::copy_nonoverlapping(frame.as_ptr().add(off), pg.va, chunk);
                }

                let i = txq.alloc();
                let d = txq.desc_va as *mut Desc;
                unsafe {
                    (*d.add(i as usize)).addr = pg.pa;
                    (*d.add(i as usize)).len = chunk as u32;
                    (*d.add(i as usize)).flags = 0;
                    (*d.add(i as usize)).next = 0;
                }
                s.tx_pages[i as usize] = Some(pg);

                if let Some(_h) = head {
                    unsafe {
                        (*d.add(prev as usize)).flags |= D_NEXT;
                        (*d.add(prev as usize)).next = i;
                    }
                    prev = i;
                } else {
                    head = Some(i);
                    prev = i;
                }

                off += chunk;
            }

            if let Some(h) = head {
                let (sel, notify_off) = (txq.sel, txq.notify_off);
                txq.push_avail(h);
                self.kick(sel, notify_off);
            }

            if off < frame.len() {
                unsafe { (*node).sent = off };
                return;
            }

            s.tx_head = unsafe { (*node).next };
            if s.tx_head.is_null() {
                s.tx_tail = core::ptr::null_mut();
            }

            kernel::free(frame.as_ptr() as *mut u8, frame.len());
            kernel::free(node as *mut u8, core::mem::size_of::<TxNode>());
        }
    }

    fn kick(&self, sel: u16, notify_off: u16) {
        let s = unsafe { &*self.state.get() };
        wmb();
        s.regs.notify(sel, notify_off);
    }
}

extern "C" fn isr_wake(token: *mut c_void, _refcon: *mut c_void, _nub: *mut c_void, _src: i32) {
    unsafe {
        if let Some(regs) = ISR_REGS {
            regs.isr_ack();
        }
    }
    A_TURN_IS_WANTED.store(true, core::sync::atomic::Ordering::Release);
    crate::nudge_the_loop(token as *const u8);
}

pub fn a_turn_is_wanted() -> bool {
    A_TURN_IS_WANTED.load(core::sync::atomic::Ordering::Acquire)
}

static A_TURN_IS_WANTED: core::sync::atomic::AtomicBool =
    core::sync::atomic::AtomicBool::new(false);

static mut ISR_REGS: Option<Regs> = None;

#[derive(Copy, Clone)]
enum Regs {
    Mmio(*mut u8),
    Pci(PciRegs),
}

#[derive(Copy, Clone)]
struct PciRegs {
    common: *mut u8,
    notify: *mut u8,
    notify_off_multiplier: u32,
    isr: *mut u8,
}

impl Regs {
    fn reset(&self) {
        match self {
            Regs::Mmio(base) => {
                w32(*base, STATUS, 0);
                w32(*base, STATUS, ST_ACK | ST_DRV);
            }
            Regs::Pci(p) => {
                w8(p.common, COMMON_STATUS, 0);
                let mut left = RESET_ATTEMPTS;
                while r8(p.common, COMMON_STATUS) != 0 {
                    left -= 1;
                    if left == 0 {
                        unsafe {
                            kernel::log("virtio: reset not taken\n");
                        }
                        break;
                    }
                }
                w8(p.common, COMMON_STATUS, (ST_ACK | ST_DRV) as u8);
            }
        }
    }

    fn feat_get(&self, sel: u32) -> u32 {
        match self {
            Regs::Mmio(base) => {
                w32(*base, DEVFEAT_SEL, sel);
                r32(*base, DEVFEAT)
            }
            Regs::Pci(p) => {
                w32(p.common, COMMON_DEVFEAT_SEL, sel);
                r32(p.common, COMMON_DEVFEAT)
            }
        }
    }

    fn feat_set(&self, sel: u32, v: u32) {
        match self {
            Regs::Mmio(base) => {
                w32(*base, DRVFEAT_SEL, sel);
                w32(*base, DRVFEAT, v);
            }
            Regs::Pci(p) => {
                w32(p.common, COMMON_DRVFEAT_SEL, sel);
                w32(p.common, COMMON_DRVFEAT, v);
            }
        }
    }

    fn status(&self) -> u32 {
        match self {
            Regs::Mmio(base) => r32(*base, STATUS),
            Regs::Pci(p) => r8(p.common, COMMON_STATUS) as u32,
        }
    }

    fn set_status(&self, v: u32) {
        match self {
            Regs::Mmio(base) => w32(*base, STATUS, v),
            Regs::Pci(p) => w8(p.common, COMMON_STATUS, v as u8),
        }
    }

    fn queue_max(&self, sel: u16) -> u16 {
        match self {
            Regs::Mmio(base) => {
                let base = *base;
                w32(base, QSEL, sel as u32);
                r32(base, QNUM_MAX) as u16
            }
            Regs::Pci(p) => {
                w16(p.common, COMMON_QSEL, sel);
                r16(p.common, COMMON_QNUM)
            }
        }
    }

    fn queue_prepare(&self, sel: u16, size: u16, desc: u64, avail: u64, used: u64) -> u16 {
        match self {
            Regs::Mmio(base) => {
                let base = *base;
                w32(base, QSEL, sel as u32);
                w32(base, QNUM, size as u32);
                w64(base, QDESC_LO, QDESC_HI, desc);
                w64(base, QAVAIL_LO, QAVAIL_HI, avail);
                w64(base, QUSED_LO, QUSED_HI, used);
                w32(base, QREADY, 1);
                0
            }
            Regs::Pci(p) => {
                w16(p.common, COMMON_QSEL, sel);
                w16(p.common, COMMON_QNUM, size);
                w64(p.common, COMMON_QDESC, COMMON_QDESC + 4, desc);
                w64(p.common, COMMON_QAVAIL, COMMON_QAVAIL + 4, avail);
                w64(p.common, COMMON_QUSED, COMMON_QUSED + 4, used);
                let notify_off = r16(p.common, COMMON_QNOTIFY_OFF);
                w16(p.common, COMMON_QREADY, 1);
                notify_off
            }
        }
    }

    fn notify(&self, sel: u16, notify_off: u16) {
        match self {
            Regs::Mmio(base) => {
                w32(*base, QSEL, sel as u32);
                w32(*base, QNOTIFY, sel as u32);
            }
            Regs::Pci(p) => {
                let off = notify_off as usize * p.notify_off_multiplier as usize;
                w16(p.notify, off, sel);
            }
        }
    }

    fn isr_ack(&self) -> u8 {
        match self {
            Regs::Mmio(base) => {
                if (r32(*base, ISR) & INT_VRING) != 0 {
                    w32(*base, ISR_ACK, INT_VRING);
                }
                0
            }
            Regs::Pci(p) => r8(p.isr, 0),
        }
    }
}

#[derive(Copy, Clone)]
struct Bus {
    number: u8,
    #[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
    config: *mut u8,
}

impl Bus {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    fn first(_ecam: u64) -> Self {
        Bus { number: 0 }
    }

    #[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
    fn first(ecam: u64) -> Self {
        let config = kernel::map_io(ecam, ECAM_BUS_SIZE) as *mut u8;
        Bus { number: 0, config }
    }

    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    fn function(&self, devfn: u8) -> PciDevice {
        PciDevice { bus: self.number, devfn }
    }

    #[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
    fn function(&self, devfn: u8) -> PciDevice {
        PciDevice {
            bus: self.number,
            devfn,
            config: unsafe { self.config.add((devfn as usize) << ECAM_DEVFN_SHIFT) },
        }
    }
}

#[derive(Copy, Clone)]
struct PciDevice {
    bus: u8,
    devfn: u8,
    #[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
    config: *mut u8,
}

impl PciDevice {
    fn find_console(ecam: u64) -> Option<Self> {
        let bus = Bus::first(ecam);

        for devfn in 0..=u8::MAX {
            let device = bus.function(devfn);

            let identity = device.read_config(0);
            let vendor = (identity & 0xffff) as u16;
            let model = (identity >> 16) as u16;
            if vendor != PCI_VENDOR_VIRTIO {
                continue;
            }
            if model != PCI_DEVICE_CONSOLE_LEGACY && model != PCI_DEVICE_CONSOLE_MODERN {
                continue;
            }

            return Some(device);
        }
        None
    }

    fn map_virtio_regs(&self) -> Option<Regs> {
        self.silence_message_interrupts();
        self.enable_memory_and_bus_mastering();
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        self.route_interrupt_line();

        let mut common = core::ptr::null_mut();
        let mut notify = core::ptr::null_mut();
        let mut notify_off_multiplier = 0;
        let mut isr = core::ptr::null_mut();

        let mut cap = (self.read_config(PCI_CAP_LIST_POINTER) & 0xfc) as u8;
        while cap != 0 {
            let header = self.read_config(cap);
            if (header & 0xff) as u8 == PCI_CAP_ID_VENDOR {
                let cfg_type = self.read_config_byte(cap + VIRTIO_CAP_CFG_TYPE);
                let bar = self.read_config_byte(cap + VIRTIO_CAP_BAR);
                let offset = self.read_config(cap + VIRTIO_CAP_OFFSET);
                let length = self.read_config(cap + VIRTIO_CAP_LENGTH);

                match cfg_type {
                    CFG_TYPE_COMMON => {
                        common = self.map_bar_region(bar, offset, length);
                    }
                    CFG_TYPE_NOTIFY => {
                        notify = self.map_bar_region(bar, offset, length);
                        notify_off_multiplier =
                            self.read_config(cap + VIRTIO_CAP_NOTIFY_MULTIPLIER);
                    }
                    CFG_TYPE_ISR => {
                        isr = self.map_bar_region(bar, offset, length);
                    }
                    _ => {}
                }
            }
            cap = ((header >> 8) & 0xfc) as u8;
        }

        if common.is_null() || notify.is_null() || isr.is_null() {
            return None;
        }

        Some(Regs::Pci(PciRegs {
            common,
            notify,
            notify_off_multiplier,
            isr,
        }))
    }

    #[cfg(feature = "linux-injected")]
    fn irq_line(&self) -> Option<u32> {
        kernel::pci_interrupt(self.bus, self.devfn)
    }

    #[cfg(not(feature = "linux-injected"))]
    fn irq_line(&self) -> Option<u32> {
        Some(self.read_config_byte(PCI_INTERRUPT_LINE) as u32)
    }

    fn silence_message_interrupts(&self) {
        let mut cap = (self.read_config(PCI_CAP_LIST_POINTER) & 0xfc) as u8;
        while cap != 0 {
            let header = self.read_config(cap);
            let cleared = match (header & 0xff) as u8 {
                PCI_CAP_ID_MSIX => header & !PCI_MSIX_ENABLE,
                PCI_CAP_ID_MSI => header & !PCI_MSI_ENABLE,
                _ => header,
            };
            if cleared != header {
                self.write_config(cap, cleared);
            }
            cap = ((header >> 8) & 0xfc) as u8;
        }
    }

    fn enable_memory_and_bus_mastering(&self) {
        let command = self.read_config(PCI_COMMAND);
        self.write_config(
            PCI_COMMAND,
            (command | PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER) & !PCI_COMMAND_INTX_DISABLE,
        );
    }

    // The chipset keeps the link of an unclaimed device off, thus the line reaches no controller.
    // Point the link at the interrupt in the config space of the device.
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    fn route_interrupt_line(&self) {
        let link = ((self.read_config_byte(PCI_INTERRUPT_PIN) - 1) + (self.devfn >> 3) - 1) & 3;
        let router = PciDevice {
            bus: 0,
            devfn: ISA_BRIDGE_DEVFN,
        };

        let offset = PIRQ_ROUTE + link;
        let shift = (offset & 3) * 8;
        let route = (self.irq_line().unwrap() & !PIRQ_DISABLED) << shift;
        let others = router.read_config(offset) & !(0xff << shift);
        router.write_config(offset, others | route);
    }

    fn map_bar_region(&self, bar: u8, offset: u32, length: u32) -> *mut u8 {
        let slot = PCI_BASE_ADDRESS_0 + bar * 4;
        let lo = self.read_config(slot);

        let is_sixty_four_bit = (lo & 0b110) == 0b100;
        let base = if is_sixty_four_bit {
            ((self.read_config(slot + 4) as u64) << 32) | ((lo & !0xf) as u64)
        } else {
            (lo & !0xf) as u64
        };

        kernel::map_io(base + offset as u64, length as u64) as *mut u8
    }

    fn read_config_byte(&self, offset: u8) -> u8 {
        (self.read_config(offset) >> ((offset & 3) * 8)) as u8
    }

    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    fn read_config(&self, offset: u8) -> u32 {
        outl(PCI_CONFIG_ADDRESS, self.config_address(offset));
        inl(PCI_CONFIG_DATA)
    }

    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    fn write_config(&self, offset: u8, value: u32) {
        outl(PCI_CONFIG_ADDRESS, self.config_address(offset));
        outl(PCI_CONFIG_DATA, value);
    }

    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    fn config_address(&self, offset: u8) -> u32 {
        0x8000_0000
            | ((self.bus as u32) << 16)
            | ((self.devfn as u32) << 8)
            | ((offset as u32) & 0xfc)
    }

    #[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
    fn read_config(&self, offset: u8) -> u32 {
        r32(self.config, (offset & 0xfc) as usize)
    }

    #[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
    fn write_config(&self, offset: u8, value: u32) {
        w32(self.config, (offset & 0xfc) as usize, value);
    }
}

fn r32(mmio: *mut u8, off: usize) -> u32 {
    unsafe { read_volatile(mmio.add(off) as *const u32) }
}

fn w32(mmio: *mut u8, off: usize, val: u32) {
    unsafe { write_volatile(mmio.add(off) as *mut u32, val) }
}

fn w64(mmio: *mut u8, lo: usize, hi: usize, v: u64) {
    w32(mmio, lo, (v & 0xffff_ffff) as u32);
    w32(mmio, hi, (v >> 32) as u32);
}

fn r16(mmio: *mut u8, off: usize) -> u16 {
    unsafe { read_volatile(mmio.add(off) as *const u16) }
}

fn w16(mmio: *mut u8, off: usize, val: u16) {
    unsafe { write_volatile(mmio.add(off) as *mut u16, val) }
}

fn r8(mmio: *mut u8, off: usize) -> u8 {
    unsafe { read_volatile(mmio.add(off)) }
}

fn w8(mmio: *mut u8, off: usize, val: u8) {
    unsafe { write_volatile(mmio.add(off), val) }
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
fn inl(port: u16) -> u32 {
    let value: u32;
    unsafe {
        core::arch::asm!("in eax, dx", out("eax") value, in("dx") port,
            options(nomem, nostack, preserves_flags));
    }
    value
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
fn outl(port: u16, value: u32) {
    unsafe {
        core::arch::asm!("out dx, eax", in("dx") port, in("eax") value,
            options(nomem, nostack, preserves_flags));
    }
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
fn wmb() {
    unsafe { core::arch::asm!("sfence", options(nostack, preserves_flags)) }
}

#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
fn wmb() {
    unsafe { core::arch::asm!("dmb ishst", options(nostack, preserves_flags)) }
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
fn rmb() {
    unsafe { core::arch::asm!("lfence", options(nostack, preserves_flags)) }
}

#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
fn rmb() {
    unsafe { core::arch::asm!("dmb ish", options(nostack, preserves_flags)) }
}
