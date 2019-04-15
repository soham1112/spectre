// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <vector>

#include "Domain/Creators/RegisterDerivedWithCharm.hpp"
#include "Domain/Tags.hpp"
#include "ErrorHandling/FloatingPointExceptions.hpp"
#include "Evolution/Actions/ComputeTimeDerivative.hpp"
#include "Evolution/Actions/ComputeVolumeFluxes.hpp"
#include "Evolution/Actions/ComputeVolumeSources.hpp"
#include "Evolution/Conservative/UpdateConservatives.hpp"
#include "Evolution/Conservative/UpdatePrimitives.hpp"
#include "Evolution/DiscontinuousGalerkin/DgElementArray.hpp"
#include "Evolution/DiscontinuousGalerkin/SlopeLimiters/LimiterActions.hpp"
#include "Evolution/DiscontinuousGalerkin/SlopeLimiters/Minmod.hpp"
#include "Evolution/DiscontinuousGalerkin/SlopeLimiters/Tags.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/FixConservatives.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/Initialize.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/NewmanHamlin.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/PalenzuelaEtAl.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/System.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/Tags.hpp"
#include "Evolution/VariableFixing/Actions.hpp"
#include "Evolution/VariableFixing/FixToAtmosphere.hpp"
#include "Evolution/VariableFixing/Tags.hpp"
#include "NumericalAlgorithms/DiscontinuousGalerkin/Actions/ApplyBoundaryFluxesLocalTimeStepping.hpp"
#include "NumericalAlgorithms/DiscontinuousGalerkin/Actions/ApplyFluxes.hpp"
#include "NumericalAlgorithms/DiscontinuousGalerkin/Actions/FluxCommunication.hpp"
#include "NumericalAlgorithms/DiscontinuousGalerkin/NumericalFluxes/LocalLaxFriedrichs.hpp"
#include "NumericalAlgorithms/DiscontinuousGalerkin/Tags.hpp"
#include "Options/Options.hpp"
#include "Parallel/GotoAction.hpp"
#include "Parallel/InitializationFunctions.hpp"
#include "Parallel/RegisterDerivedClassesWithCharm.hpp"
#include "PointwiseFunctions/AnalyticData/GrMhd/CylindricalBlastWave.hpp"
#include "PointwiseFunctions/AnalyticData/Tags.hpp"
#include "PointwiseFunctions/Hydro/Tags.hpp"
#include "Time/Actions/AdvanceTime.hpp"
#include "Time/Actions/ChangeStepSize.hpp"
#include "Time/Actions/FinalTime.hpp"
#include "Time/Actions/RecordTimeStepperData.hpp"
#include "Time/Actions/SelfStartActions.hpp"  // IWYU pragma: keep
#include "Time/Actions/UpdateU.hpp"
#include "Time/StepChoosers/Cfl.hpp"
#include "Time/StepChoosers/Constant.hpp"
#include "Time/StepChoosers/Increase.hpp"
#include "Time/StepChoosers/StepChooser.hpp"
#include "Time/StepControllers/StepController.hpp"
#include "Time/Tags.hpp"
#include "Time/TimeSteppers/TimeStepper.hpp"
#include "Utilities/Functional.hpp"
#include "Utilities/TMPL.hpp"

/// \cond
namespace Frame {
struct Inertial;
}  // namespace Frame
namespace Parallel {
template <typename Metavariables>
class CProxy_ConstGlobalCache;
}  // namespace Parallel
/// \endcond

struct EvolutionMetavars {
  using analytic_data = grmhd::AnalyticData::CylindricalBlastWave;

  using system = grmhd::ValenciaDivClean::System<
      typename analytic_data::equation_of_state_type>;
  static constexpr size_t thermodynamic_dim = system::thermodynamic_dim;
  using temporal_id = Tags::TimeId;
  static constexpr bool local_time_stepping = false;
  using analytic_data_tag = OptionTags::AnalyticData<analytic_data>;
  using analytic_variables_tags =
      typename system::primitive_variables_tag::tags_list;
  using equation_of_state_tag = hydro::Tags::EquationOfState<
      typename analytic_data_tag::type::equation_of_state_type>;
  using normal_dot_numerical_flux = OptionTags::NumericalFluxParams<
      dg::NumericalFluxes::LocalLaxFriedrichs<system>>;
  // GRMHD is only implemented in 3D.
  // Do not limit the divergence-cleaning field Phi
  using limiter = OptionTags::SlopeLimiterParams<SlopeLimiters::Minmod<
      3, tmpl::list<grmhd::ValenciaDivClean::Tags::TildeD,
                    grmhd::ValenciaDivClean::Tags::TildeTau,
                    grmhd::ValenciaDivClean::Tags::TildeS<Frame::Inertial>,
                    grmhd::ValenciaDivClean::Tags::TildeB<Frame::Inertial>>>>;
  using step_choosers =
      tmpl::list<StepChoosers::Registrars::Cfl<3, Frame::Inertial>,
                 StepChoosers::Registrars::Constant,
                 StepChoosers::Registrars::Increase>;
  using ordered_list_of_primitive_recovery_schemes = tmpl::list<
      grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::NewmanHamlin,
      grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::PalenzuelaEtAl>;

  using compute_rhs = tmpl::flatten<
      tmpl::list<Actions::ComputeVolumeFluxes,
                 dg::Actions::SendDataForFluxes<EvolutionMetavars>,
                 Actions::ComputeVolumeSources, Actions::ComputeTimeDerivative,
                 dg::Actions::ReceiveDataForFluxes<EvolutionMetavars>,
                 tmpl::conditional_t<local_time_stepping, tmpl::list<>,
                                     dg::Actions::ApplyFluxes>,
                 Actions::RecordTimeStepperData>>;

  using update_variables = tmpl::flatten<tmpl::list<
      tmpl::conditional_t<local_time_stepping,
                          dg::Actions::ApplyBoundaryFluxesLocalTimeStepping,
                          tmpl::list<>>,
      Actions::UpdateU, SlopeLimiters::Actions::SendData<EvolutionMetavars>,
      SlopeLimiters::Actions::Limit<EvolutionMetavars>,
      VariableFixing::Actions::FixVariables<VariableFixing::FixConservatives>,
      Actions::UpdatePrimitives>>;

  struct EvolvePhaseStart;
  using component_list = tmpl::list<DgElementArray<
      EvolutionMetavars, grmhd::ValenciaDivClean::Actions::Initialize<3>,
      tmpl::flatten<tmpl::list<
          VariableFixing::Actions::FixVariables<
              VariableFixing::FixToAtmosphere<thermodynamic_dim>>,
          Actions::UpdateConservatives,
          SelfStart::self_start_procedure<compute_rhs, update_variables>,
          Actions::Label<EvolvePhaseStart>, Actions::AdvanceTime,
          VariableFixing::Actions::FixVariables<
              VariableFixing::FixToAtmosphere<thermodynamic_dim>>,
          Actions::UpdateConservatives, Actions::FinalTime,
          tmpl::conditional_t<local_time_stepping,
                              Actions::ChangeStepSize<step_choosers>,
                              tmpl::list<>>,
          compute_rhs, update_variables, Actions::Goto<EvolvePhaseStart>>>>>;

  using const_global_cache_tag_list =
      tmpl::list<analytic_data_tag,
                 OptionTags::TypedTimeStepper<tmpl::conditional_t<
                     local_time_stepping, LtsTimeStepper, TimeStepper>>,
                 OptionTags::DampingParameter>;

  using domain_creator_tag = OptionTags::DomainCreator<3, Frame::Inertial>;

  static constexpr OptionString help{
      "Evolve analytic data using the Valencia formulation of the GRMHD system "
      "with divergence cleaning.\n\n"
      "Analytic data: cylindrical blast wave\n"
      "Slope limiter: Minmod family\n"
      "Numerical flux: Local Lax-Friedrichs\n"
      "Variable fixing: FixConservatives and FixToAtmosphere\n"
      "Local time-stepping: none\n"
      "Boundary conditions: only periodic are currently supported\n"};

  enum class Phase { Initialization, Evolve, Exit };

  static Phase determine_next_phase(
      const Phase& current_phase,
      const Parallel::CProxy_ConstGlobalCache<
          EvolutionMetavars>& /*cache_proxy*/) noexcept {
    switch (current_phase) {
      case Phase::Initialization:
        return Phase::Evolve;
      case Phase::Evolve:
        return Phase::Exit;
      case Phase::Exit:
        ERROR(
            "Should never call determine_next_phase with the current phase "
            "being 'Exit'");
      default:
        ERROR(
            "Unknown type of phase. Did you static_cast<Phase> an integral "
            "value?");
    }
  }
};

static const std::vector<void (*)()> charm_init_node_funcs{
    &setup_error_handling, &domain::creators::register_derived_with_charm,
    &Parallel::register_derived_classes_with_charm<
        StepChooser<EvolutionMetavars::step_choosers>>,
    &Parallel::register_derived_classes_with_charm<StepController>,
    &Parallel::register_derived_classes_with_charm<TimeStepper>};

static const std::vector<void (*)()> charm_init_proc_funcs{
    &enable_floating_point_exceptions};
